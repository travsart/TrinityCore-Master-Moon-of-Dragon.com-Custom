"""
5-Round Review Cycle — Orchestrates Codex, Gemini, Claude API, and ChatGPT reviews.

Architecture (parallel-first, sequential-verify):
  Phase 1 (PARALLEL):    Codex + Gemini + Claude review artifact simultaneously
  Phase 2 (SEQUENTIAL):  Codex verifies fixes using all Phase 1 feedback
  Phase 3 (SEQUENTIAL):  Gemini final seal with all prior feedback

  Old sequential pipeline: ~45 min
  New parallel pipeline:   ~15 min  (3x faster)

Codex rounds use the CLI (ChatGPT Pro subscription, flat rate, $0 marginal).
Gemini and Claude rounds use their respective APIs (per-token).
ChatGPT API remains available as a fallback via --use-chatgpt-api.

Usage:
    python review_cycle.py --file path/to/artifact.md
    python review_cycle.py --file path/to/artifact.md --rounds 3
    python review_cycle.py --file path/to/artifact.md --skip-claude
    python review_cycle.py --file path/to/artifact.md --use-chatgpt-api
    python review_cycle.py --file path/to/artifact.md --sequential
    python review_cycle.py --test
"""
import os
import sys
import argparse
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from datetime import datetime

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
REPORTS_DIR = PROJECT_ROOT / "AI_Studio" / "Reports" / "Audits"

sys.path.insert(0, str(SCRIPT_DIR))

# Models — strongest available for each provider
MODELS = {
    "codex": os.environ.get("REVIEW_MODEL_CODEX", "gpt-5.4"),
    "chatgpt": os.environ.get("REVIEW_MODEL_CHATGPT", "gpt-5.4"),
    "gemini": os.environ.get("REVIEW_MODEL_GEMINI", "gemini-2.5-pro"),
    "claude": os.environ.get("REVIEW_MODEL_CLAUDE", "claude-sonnet-4-6"),
}

# --- Context file loading ---

def load_context_files(file_paths: list[str] | None) -> str:
    """Load supplementary context files and return formatted text block.

    Returns empty string if no files provided. The assembled context is also
    saved to AI_Studio/Reports/Audits/ for compaction-safe persistence.
    """
    if not file_paths:
        return ""

    parts = []
    loaded = []
    for fp in file_paths:
        p = Path(fp)
        if not p.is_absolute():
            p = PROJECT_ROOT / p
        if not p.exists():
            print(f"  WARNING: Context file not found, skipping: {fp}")
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        parts.append(f"--- PROJECT CONTEXT: {p.name} ---\n{text}\n--- END {p.name} ---\n")
        loaded.append(str(p))
        print(f"  Loaded context: {p.name} ({len(text):,} chars)")

    if not parts:
        return ""

    assembled = "\n".join(parts) + "\n"

    # Save assembled context for compaction-safe persistence
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    ctx_path = REPORTS_DIR / f"{timestamp}__REVIEW_CONTEXT.md"
    ctx_path.write_text(
        f"# Review Cycle Context Files\n\n"
        f"**Date**: {datetime.now().isoformat()}\n"
        f"**Files**: {', '.join(loaded)}\n"
        f"**Total chars**: {len(assembled):,}\n\n"
        + assembled,
        encoding="utf-8",
    )
    print(f"  Context saved: {ctx_path.name} ({len(assembled):,} chars total)\n")

    return assembled


# --- Phase definitions ---
# Phase 1 runs in PARALLEL (no prior feedback needed — fresh eyes on artifact)
# Phase 2-3 run SEQUENTIALLY (need Phase 1 results as input)

PHASE1_CODEX = [
    ("Codex",  "codex",  "repo-aware architecture/design review"),
    ("Gemini", "gemini", "correctness audit, edge cases, security"),
    ("Claude", "claude", "cold-read, implementation bias detection"),
]

PHASE1_API = [
    ("ChatGPT", "chatgpt", "architecture/design review"),
    ("Gemini",  "gemini",  "correctness audit, edge cases, security"),
    ("Claude",  "claude",  "cold-read, implementation bias detection"),
]

PHASE2_CODEX = ("Codex",   "codex",  "verify Phase 1 findings by reading actual code")
PHASE2_API   = ("ChatGPT", "chatgpt", "verify Phase 1 findings, design coherence")

PHASE3 = ("Gemini", "gemini", "final seal — strictest auditor gets last word")

# Legacy sequential plans (for --sequential flag)
ROUND_PLAN_CODEX = [
    ("Codex",    "codex",   "repo-aware architecture/design review"),
    ("Gemini",   "gemini",  "correctness audit, edge cases, security"),
    ("Claude",   "claude",  "cold-read, implementation bias detection"),
    ("Codex",    "codex",   "verify fixes from rounds 2-3 by reading actual code"),
    ("Gemini",   "gemini",  "final seal — strictest auditor gets last word"),
]

ROUND_PLAN_API = [
    ("ChatGPT",  "chatgpt", "architecture/design review"),
    ("Gemini",   "gemini",  "correctness audit, edge cases, security"),
    ("Claude",   "claude",  "cold-read, implementation bias detection"),
    ("ChatGPT",  "chatgpt", "verify fixes from rounds 2-3, design coherence"),
    ("Gemini",   "gemini",  "final seal — strictest auditor gets last word"),
]


# --- Dispatchers ---

def call_codex(artifact: str, round_num: int, prior_feedback: str,
               role: str, model: str) -> str:
    from call_codex_review import review
    return review(artifact, round_num, prior_feedback, role, model)


def call_chatgpt(artifact: str, round_num: int, prior_feedback: str,
                 role: str, model: str) -> str:
    from call_chatgpt_review import review
    return review(artifact, round_num, prior_feedback, role, model)


def call_gemini(artifact: str, round_num: int, prior_feedback: str,
                role: str, model: str) -> str:
    from call_gemini import review
    return review(artifact, round_num, prior_feedback, role, model)


def call_claude(artifact: str, round_num: int, prior_feedback: str,
                role: str, model: str) -> str:
    from call_claude import review
    return review(artifact, round_num, prior_feedback, role, model)


DISPATCHERS = {
    "codex": call_codex,
    "chatgpt": call_chatgpt,
    "gemini": call_gemini,
    "claude": call_claude,
}


# --- File I/O helpers ---

def save_round_review(artifact_name: str, round_num: int, reviewer: str,
                      review_text: str, model: str, elapsed: float) -> Path:
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    stem = Path(artifact_name).stem
    filename = f"{timestamp}__REVIEW_R{round_num}_{reviewer}_{stem}.md"
    out_path = REPORTS_DIR / filename

    header = (
        f"---\n"
        f"artifact: {artifact_name}\n"
        f"round: {round_num}\n"
        f"reviewer: {reviewer}\n"
        f"model: {model}\n"
        f"date: {datetime.now().isoformat()}\n"
        f"elapsed_seconds: {elapsed:.1f}\n"
        f"---\n\n"
    )

    out_path.write_text(header + review_text, encoding="utf-8")
    return out_path


def save_cycle_summary(artifact_name: str, rounds_completed: list,
                       final_verdict: str, wall_time: float) -> Path:
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    stem = Path(artifact_name).stem
    filename = f"{timestamp}__REVIEW_SUMMARY_{stem}.md"
    out_path = REPORTS_DIR / filename

    cpu_time = sum(r["elapsed"] for r in rounds_completed)

    lines = [
        f"# Review Cycle Summary: {artifact_name}\n",
        f"**Date**: {datetime.now().isoformat()}\n",
        f"**Rounds completed**: {len(rounds_completed)}\n",
        f"**Final verdict**: {final_verdict}\n",
        f"**Wall time**: {wall_time:.1f}s | **CPU time**: {cpu_time:.1f}s "
        f"(saved {cpu_time - wall_time:.0f}s via parallelism)\n",
        "",
        "## Round Results\n",
    ]

    for r in rounds_completed:
        verdict_marker = "PASS" if "pass" in r["verdict"].lower() else "FAIL"
        phase = r.get("phase", "?")
        lines.append(
            f"| R{r['round']} | {r['reviewer']} | {r['model']} | "
            f"{r['elapsed']:.1f}s | {verdict_marker} | Phase {phase} |"
        )

    lines.insert(7, "| Round | Reviewer | Model | Time | Verdict | Phase |")
    lines.insert(8, "|-------|----------|-------|------|---------|-------|")

    lines.append("")
    lines.append("## Per-Round Reviews\n")
    for r in rounds_completed:
        lines.append(f"### Round {r['round']}: {r['reviewer']} (Phase {r.get('phase', '?')})\n")
        lines.append(r["review_text"])
        lines.append("\n---\n")

    out_path.write_text("\n".join(lines), encoding="utf-8")
    return out_path


def extract_verdict(review_text: str) -> str:
    upper = review_text.upper()
    for line in upper.split("\n"):
        if "VERDICT" in line:
            if "FAIL" in line:
                return "FAIL"
            if "PASS" in line:
                return "PASS"
    critical = upper.count("CRITICAL")
    high = upper.count("[HIGH]")
    if critical > 0 or high > 0:
        return "FAIL"
    return "PASS"


def condense_feedback(rounds_completed: list) -> str:
    """Build a condensed summary of prior rounds for Phase 2-3 prompts.

    Instead of dumping ALL review text verbatim (which balloons the prompt),
    extract just the findings and verdicts — typically 30-50% smaller.
    """
    parts = []
    for r in rounds_completed:
        text = r["review_text"]
        # Extract just the findings (lines with severity markers) + verdict
        important_lines = []
        for line in text.split("\n"):
            line_upper = line.upper()
            if any(marker in line_upper for marker in
                   ["CRITICAL", "[HIGH]", "[MEDIUM]", "[LOW]", "[INFO]",
                    "VERDICT", "**VERDICT", "PASS", "FAIL"]):
                important_lines.append(line)
        if not important_lines:
            # Fallback: use full text if no structured findings detected
            important_lines = [text[:2000] + "..." if len(text) > 2000 else text]

        parts.append(
            f"### {r['reviewer']} ({r['model']}) — Verdict: {r['verdict']}\n"
            + "\n".join(important_lines)
        )
    return "\n\n".join(parts)


# --- Run a single round (used by both parallel and sequential paths) ---

def _run_one_round(artifact_name: str, artifact_text: str, round_num: int,
                   reviewer_name: str, module_key: str, role_desc: str,
                   prior_feedback: str, phase: int) -> dict:
    model = MODELS[module_key]
    dispatcher = DISPATCHERS[module_key]
    start = time.time()

    try:
        review_text = dispatcher(
            artifact=artifact_text,
            round_num=round_num,
            prior_feedback=prior_feedback,
            role=role_desc,
            model=model,
        )
    except Exception as e:
        review_text = f"**ERROR**: {reviewer_name} call failed: {e}"

    elapsed = time.time() - start
    verdict = extract_verdict(review_text)

    review_path = save_round_review(artifact_name, round_num, reviewer_name,
                                    review_text, model, elapsed)

    return {
        "round": round_num,
        "reviewer": reviewer_name,
        "model": model,
        "elapsed": elapsed,
        "verdict": verdict,
        "review_text": review_text,
        "review_path": str(review_path),
        "phase": phase,
    }


# --- Parallel pipeline (default) ---

def run_cycle_parallel(artifact_path: Path, max_rounds: int = 5,
                       skip_claude: bool = False, use_chatgpt_api: bool = False,
                       context_text: str = ""):
    """Run the parallel-first review pipeline.

    Phase 1: 3 reviewers in parallel (no prior feedback)
    Phase 2: Verification round (sequential, gets Phase 1 condensed feedback)
    Phase 3: Final seal (sequential, gets all condensed feedback)
    """
    artifact_name = artifact_path.name
    raw_artifact = artifact_path.read_text(encoding="utf-8")
    # Prepend context files so reviewers have project knowledge
    artifact_text = context_text + raw_artifact if context_text else raw_artifact
    wall_start = time.time()

    phase1_plan = PHASE1_API if use_chatgpt_api else PHASE1_CODEX
    phase2_def = PHASE2_API if use_chatgpt_api else PHASE2_CODEX

    if skip_claude:
        phase1_plan = [(n, m, r) for n, m, r in phase1_plan if m != "claude"]

    # Clamp total rounds
    total_planned = len(phase1_plan) + 2  # Phase 1 + Phase 2 + Phase 3
    if max_rounds < total_planned:
        # Trim: first cut Phase 3, then Phase 2, then Phase 1 reviewers
        if max_rounds <= len(phase1_plan):
            phase1_plan = phase1_plan[:max_rounds]
            phase2_def = None
            phase3_def = None
        elif max_rounds == len(phase1_plan) + 1:
            phase3_def = None
        else:
            phase3_def = PHASE3
    else:
        phase3_def = PHASE3

    all_reviewers = [n for n, _, _ in phase1_plan]
    if phase2_def:
        all_reviewers.append(phase2_def[0])
    if phase3_def:
        all_reviewers.append(phase3_def[0])

    print(f"\n{'='*70}")
    print(f"  REVIEW CYCLE (parallel): {artifact_name}")
    print(f"  Phase 1: {', '.join(n for n, _, _ in phase1_plan)} (parallel)")
    if phase2_def:
        print(f"  Phase 2: {phase2_def[0]} (verify)")
    if phase3_def:
        print(f"  Phase 3: {phase3_def[0]} (final seal)")
    print(f"{'='*70}\n")

    rounds_completed = []
    round_counter = 0

    # --- PHASE 1: Parallel ---
    print(f"  Phase 1 — launching {len(phase1_plan)} reviewers in parallel...")
    phase1_results = []

    with ThreadPoolExecutor(max_workers=len(phase1_plan)) as pool:
        futures = {}
        for reviewer_name, module_key, role_desc in phase1_plan:
            round_counter += 1
            rnum = round_counter
            future = pool.submit(
                _run_one_round,
                artifact_name, artifact_text, rnum,
                reviewer_name, module_key, role_desc,
                "",  # no prior feedback in Phase 1
                1,   # phase number
            )
            futures[future] = (rnum, reviewer_name)

        for future in as_completed(futures):
            rnum, rname = futures[future]
            result = future.result()
            phase1_results.append(result)
            rounds_completed.append(result)
            print(f"    R{rnum} {rname}: {result['verdict']} | "
                  f"{result['elapsed']:.1f}s | {Path(result['review_path']).name}")

    # Sort by round number for consistent ordering
    phase1_results.sort(key=lambda r: r["round"])
    rounds_completed.sort(key=lambda r: r["round"])

    phase1_time = max(r["elapsed"] for r in phase1_results)
    print(f"  Phase 1 complete — wall time: {phase1_time:.1f}s "
          f"(longest of {len(phase1_results)} parallel reviewers)\n")

    # Build condensed feedback from Phase 1
    condensed = condense_feedback(phase1_results)

    # --- PHASE 2: Sequential verification ---
    if phase2_def:
        reviewer_name, module_key, role_desc = phase2_def
        round_counter += 1
        print(f"  Phase 2 — {reviewer_name} verifying Phase 1 findings...")
        result = _run_one_round(
            artifact_name, artifact_text, round_counter,
            reviewer_name, module_key, role_desc,
            condensed, 2,
        )
        rounds_completed.append(result)
        print(f"    R{round_counter} {reviewer_name}: {result['verdict']} | "
              f"{result['elapsed']:.1f}s | {Path(result['review_path']).name}\n")

        # Update condensed feedback with Phase 2 results
        condensed = condense_feedback(rounds_completed)

    # --- PHASE 3: Final seal ---
    if phase3_def:
        reviewer_name, module_key, role_desc = phase3_def
        round_counter += 1
        print(f"  Phase 3 — {reviewer_name} final seal...")
        result = _run_one_round(
            artifact_name, artifact_text, round_counter,
            reviewer_name, module_key, role_desc,
            condensed, 3,
        )
        rounds_completed.append(result)
        print(f"    R{round_counter} {reviewer_name}: {result['verdict']} | "
              f"{result['elapsed']:.1f}s | {Path(result['review_path']).name}\n")

    # --- Summary ---
    wall_time = time.time() - wall_start
    final_verdict = rounds_completed[-1]["verdict"] if rounds_completed else "UNKNOWN"
    summary_path = save_cycle_summary(artifact_name, rounds_completed,
                                      final_verdict, wall_time)

    cpu_time = sum(r["elapsed"] for r in rounds_completed)

    print(f"{'='*70}")
    print(f"  CYCLE COMPLETE: {final_verdict}")
    print(f"  Wall time: {wall_time:.1f}s | CPU time: {cpu_time:.1f}s | "
          f"Saved: {cpu_time - wall_time:.0f}s via parallelism")
    print(f"  Summary: {summary_path}")
    print(f"{'='*70}\n")

    return {
        "final_verdict": final_verdict,
        "rounds": rounds_completed,
        "summary_path": str(summary_path),
        "wall_time": wall_time,
        "cpu_time": cpu_time,
    }


# --- Legacy sequential pipeline (--sequential flag) ---

def run_cycle_sequential(artifact_path: Path, max_rounds: int = 5,
                         skip_claude: bool = False, use_chatgpt_api: bool = False,
                         context_text: str = ""):
    """Run the old fully-sequential review pipeline."""
    artifact_name = artifact_path.name
    raw_artifact = artifact_path.read_text(encoding="utf-8")
    artifact_text = context_text + raw_artifact if context_text else raw_artifact
    wall_start = time.time()

    base_plan = ROUND_PLAN_API if use_chatgpt_api else ROUND_PLAN_CODEX
    plan = base_plan[:max_rounds]

    if skip_claude:
        plan = [(name, mod, role) for name, mod, role in plan if mod != "claude"]
        plan = plan[:max_rounds]

    print(f"\n{'='*70}")
    print(f"  REVIEW CYCLE (sequential): {artifact_name}")
    print(f"  Rounds: {len(plan)} | Models: {', '.join(set(MODELS[p[1]] for p in plan))}")
    print(f"{'='*70}\n")

    rounds_completed = []
    cumulative_feedback = ""

    for i, (reviewer_name, module_key, role_desc) in enumerate(plan, 1):
        model = MODELS[module_key]
        print(f"  Round {i}/{len(plan)}: {reviewer_name} ({model}) — {role_desc}")

        result = _run_one_round(
            artifact_name, artifact_text, i,
            reviewer_name, module_key, role_desc,
            cumulative_feedback, i,
        )
        rounds_completed.append(result)
        print(f"    Verdict: {result['verdict']} | {result['elapsed']:.1f}s | "
              f"Saved: {Path(result['review_path']).name}")

        cumulative_feedback += (
            f"\n\n### Round {i} ({reviewer_name}, {model}):\n"
            f"Verdict: {result['verdict']}\n\n{result['review_text']}"
        )

    wall_time = time.time() - wall_start
    final_verdict = rounds_completed[-1]["verdict"] if rounds_completed else "UNKNOWN"
    summary_path = save_cycle_summary(artifact_name, rounds_completed,
                                      final_verdict, wall_time)

    print(f"\n{'='*70}")
    print(f"  CYCLE COMPLETE: {final_verdict}")
    print(f"  Wall time: {wall_time:.1f}s")
    print(f"  Summary: {summary_path}")
    print(f"{'='*70}\n")

    return {
        "final_verdict": final_verdict,
        "rounds": rounds_completed,
        "summary_path": str(summary_path),
        "wall_time": wall_time,
        "cpu_time": wall_time,
    }


# --- Tests ---

def test_all():
    """Test connectivity to all 4 reviewer endpoints (parallel)."""
    print("Testing all 4 reviewer endpoints in parallel...\n")
    results = {}

    def _test(name, import_fn):
        try:
            fn = import_fn()
            return (name, fn())
        except Exception as e:
            print(f"  {name} FAILED: {e}")
            return (name, False)

    with ThreadPoolExecutor(max_workers=4) as pool:
        futures = [
            pool.submit(_test, "Codex", lambda: __import__("call_codex_review").test_connection),
            pool.submit(_test, "ChatGPT", lambda: __import__("call_chatgpt_review").test_connection),
            pool.submit(_test, "Gemini", lambda: __import__("call_gemini").test_connection),
            pool.submit(_test, "Claude", lambda: __import__("call_claude").test_connection),
        ]
        for f in as_completed(futures):
            name, ok = f.result()
            results[name] = ok

    print(f"\n{'='*40}")
    for name in ["Codex", "ChatGPT", "Gemini", "Claude"]:
        status = "OK" if results.get(name) else "FAILED"
        print(f"  {name}: {status}")
    print(f"{'='*40}")

    all_ok = all(results.values())
    if all_ok:
        print("\nAll 4 endpoints operational. Parallel pipeline ready.")
    else:
        failed = [k for k, v in results.items() if not v]
        print(f"\nFailed: {', '.join(failed)}.")

    return all_ok


# --- CLI ---

def main():
    parser = argparse.ArgumentParser(
        description="5-Round Review Cycle — Parallel-first with Codex, Gemini, Claude"
    )
    parser.add_argument("--test", action="store_true",
                        help="Test connectivity to all 4 endpoints")
    parser.add_argument("--file", type=str,
                        help="Path to artifact to review")
    parser.add_argument("--rounds", type=int, default=5,
                        help="Max review rounds (1-5, default: 5)")
    parser.add_argument("--skip-claude", action="store_true",
                        help="Skip Claude API rounds")
    parser.add_argument("--use-chatgpt-api", action="store_true",
                        help="Use ChatGPT API instead of Codex CLI for rounds 1 & 4")
    parser.add_argument("--sequential", action="store_true",
                        help="Use old sequential pipeline (slower, full feedback chain)")
    parser.add_argument("--context-files", nargs="+", type=str, metavar="FILE",
                        help="Supplementary context files to prepend to the artifact "
                             "(e.g., CLAUDE.md, intake docs). Reviewers see these as "
                             "project context before the artifact itself.")
    args = parser.parse_args()

    if args.test:
        sys.exit(0 if test_all() else 1)

    if args.file:
        path = Path(args.file)
        if not path.exists():
            print(f"ERROR: File not found: {args.file}")
            sys.exit(1)

        # Load and prepend context files if provided
        context_text = load_context_files(args.context_files)

        if args.sequential:
            result = run_cycle_sequential(
                path, max_rounds=args.rounds,
                skip_claude=args.skip_claude,
                use_chatgpt_api=args.use_chatgpt_api,
                context_text=context_text)
        else:
            result = run_cycle_parallel(
                path, max_rounds=args.rounds,
                skip_claude=args.skip_claude,
                use_chatgpt_api=args.use_chatgpt_api,
                context_text=context_text)

        sys.exit(0 if result["final_verdict"] == "PASS" else 1)

    parser.print_help()


if __name__ == "__main__":
    main()
