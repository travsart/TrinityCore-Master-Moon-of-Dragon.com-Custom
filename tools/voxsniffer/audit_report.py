"""VoxSniffer Combat Audit Report Generator.

Reads CombatAudit observations from VoxSniffer SavedVariables and generates
a Markdown diagnostic report for Claude Code spell verification.

Usage:
    python tools/voxsniffer/audit_report.py --input <SavedVariables.lua> [--output <report.md>]
    python tools/voxsniffer/audit_report.py --input <SavedVariables.lua> --session <id>
"""

import argparse
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

# Resolve imports whether run as module or standalone
try:
    from .parsers.savedvariables_loader import SavedVariablesLoader, SchemaError
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from voxsniffer.parsers.savedvariables_loader import SavedVariablesLoader, SchemaError


# Observation type constants (must match Lua C.OBS_TYPE)
OBS_TYPES = {
    "talent_snapshot": "talent_snapshot",
    "player_combat": "player_combat",
    "aura_timeline": "aura_timeline",
    "spell_stats": "spell_stats",
    "proc_fulfilled": "proc_fulfilled",
    "proc_timeout": "proc_timeout",
    "talent_anomaly": "talent_anomaly",
    "audit_summary": "audit_summary",
}

MODULE_NAME = "CombatAudit"


def extract_audit_records(
    loader: SavedVariablesLoader,
    session_id: int | None = None,
) -> list[dict]:
    """Extract all CombatAudit records from loaded SavedVariables."""
    records = loader.get_all_records(session_id=session_id, module=MODULE_NAME)
    return [r for r in records if isinstance(r, dict)]


def group_by_obs_type(records: list[dict]) -> dict[str, list[dict]]:
    """Group records by observation type (the 't' field in envelopes)."""
    groups: dict[str, list[dict]] = {}
    for rec in records:
        obs_type = rec.get("t", "unknown")
        groups.setdefault(obs_type, []).append(rec)
    return groups


def render_session_metadata(
    summary: dict | None,
    snapshot: dict | None,
    total_records: int,
) -> str:
    """Render the session metadata section."""
    lines = ["## Session Metadata\n"]

    if summary:
        p = summary.get("p", {})
        lines.append(f"- **Class**: {p.get('classToken', '?')}")
        lines.append(f"- **Spec**: {p.get('specName', '?')}")
        lines.append(f"- **Player**: {p.get('playerName', '?')} @ {p.get('realm', '?')}")
        lines.append(f"- **Duration**: {p.get('durationFormatted', '?')}")
        lines.append(f"- **Total Events**: {p.get('totalEvents', 0):,}")
        lines.append(f"- **Procs Expected**: {p.get('procsExpected', 0)}")
        lines.append(f"- **Procs Fulfilled**: {p.get('procsFulfilled', 0)}")
        lines.append(f"- **Procs Timed Out**: {p.get('procsTimedOut', 0)}")
        lines.append(f"- **Anomalies Detected**: {p.get('anomaliesDetected', 0)}")
    else:
        lines.append("- *No audit summary found (session may not have been stopped cleanly)*")

    if snapshot:
        sp = snapshot.get("p", {})
        talent_count = sp.get("talentCount", 0)
        hero_count = sp.get("heroTalentCount", 0)
        lines.append(f"- **Talents Captured**: {talent_count} class/spec + {hero_count} hero")
        stats = sp.get("stats", {})
        if stats:
            lines.append(f"- **Spell Power**: {stats.get('spellPower', '?')}")
            lines.append(f"- **Haste**: {stats.get('hastePercent', '?')}%")
            lines.append(f"- **Crit**: {stats.get('critChance', '?')}%")
            lines.append(f"- **Mastery**: {stats.get('masteryEffect', '?')}")

    lines.append(f"- **Total Audit Records**: {total_records:,}")
    lines.append("")
    return "\n".join(lines)


def render_proc_timeouts(timeouts: list[dict]) -> str:
    """Render the proc timeouts section — the most important part."""
    if not timeouts:
        return "## Proc Timeouts\n\nNo proc timeouts detected. All configured expectations were fulfilled.\n"

    lines = ["## Proc Timeouts (Likely Broken)\n"]
    lines.append("These proc chains were triggered enough times but the expected proc never appeared.\n")
    lines.append("| Trigger Spell | Expected Proc | Triggers | Expected % | Window | Confidence | Notes |")
    lines.append("|---|---|---|---|---|---|---|")

    for rec in timeouts:
        p = rec.get("p", {})
        trigger = p.get("triggerSpellId", "?")
        expected = p.get("expectedSpellId", "?")
        count = p.get("triggerCount", 0)
        chance = p.get("expectedChance", 0)
        window = p.get("windowElapsed", p.get("windowSeconds", "?"))
        confidence = p.get("confidence", "medium")
        notes = p.get("notes", "")

        conf_marker = "HIGH" if confidence == "high" else "MEDIUM"
        lines.append(
            f"| {trigger} | {expected} | {count} | {chance*100:.0f}% | {window:.0f}s | {conf_marker} | {notes} |"
        )

    lines.append("")

    # High-confidence callout
    high_conf = [r for r in timeouts if r.get("p", {}).get("confidence") == "high"]
    if high_conf:
        lines.append(f"**{len(high_conf)} HIGH confidence timeout(s)** — these are deterministic procs "
                      "that should fire every time. Their absence strongly suggests a broken spell handler.\n")

    return "\n".join(lines)


def render_proc_fulfilled(fulfilled: list[dict]) -> str:
    """Render fulfilled procs with latency info."""
    if not fulfilled:
        return ""

    lines = ["## Proc Fulfilled (Working)\n"]
    lines.append("| Trigger Spell | Proc Spell | Latency (ms) | Triggers | Notes |")
    lines.append("|---|---|---|---|---|")

    for rec in fulfilled:
        p = rec.get("p", {})
        lines.append(
            f"| {p.get('triggerSpellId', '?')} | {p.get('expectedSpellId', '?')} "
            f"| {p.get('latencyMs', '?'):.0f} | {p.get('triggerCount', '?')} "
            f"| {p.get('notes', '')} |"
        )

    lines.append("")
    return "\n".join(lines)


def render_spell_stats(stats_records: list[dict]) -> str:
    """Render spell statistics table — last flush per spell."""
    if not stats_records:
        return "## Spell Statistics\n\nNo spell statistics recorded.\n"

    # Use the latest stats per spellId (cumulative from session)
    latest: dict[int, dict] = {}
    for rec in stats_records:
        p = rec.get("p", {})
        spell_id = p.get("spellId")
        if spell_id is not None:
            latest[spell_id] = p

    if not latest:
        return "## Spell Statistics\n\nNo spell statistics recorded.\n"

    # Sort by total output (damage + healing) descending
    sorted_spells = sorted(
        latest.values(),
        key=lambda s: (s.get("totalDamage", 0) + s.get("totalHealing", 0)),
        reverse=True,
    )

    lines = ["## Spell Statistics\n"]
    lines.append("| Spell | ID | Casts | Hits | Crits | Crit% | Miss | Total Dmg | Total Heal | Min | Max |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|")

    for sp in sorted_spells[:30]:  # Top 30
        hit_count = sp.get("hitCount", 0)
        crit_count = sp.get("critCount", 0)
        crit_pct = (crit_count / hit_count * 100) if hit_count > 0 else 0
        lines.append(
            f"| {sp.get('spellName', '?')} | {sp.get('spellId', '?')} "
            f"| {sp.get('castCount', 0)} | {hit_count} | {crit_count} "
            f"| {crit_pct:.1f}% | {sp.get('missCount', 0)} "
            f"| {sp.get('totalDamage', 0):,} | {sp.get('totalHealing', 0):,} "
            f"| {sp.get('minHit', '-')} | {sp.get('maxHit', '-')} |"
        )

    lines.append("")

    # Player stats context from the latest flush
    if sorted_spells:
        ps = sorted_spells[0].get("playerStats", {})
        if ps:
            lines.append("**Player Stats at Last Flush:**")
            lines.append(f"- Spell Power: {ps.get('spellPower', '?')} | "
                          f"AP: {ps.get('attackPower', '?')} | "
                          f"Crit: {ps.get('critChance', '?')}% | "
                          f"Haste: {ps.get('hastePercent', '?')}% | "
                          f"Mastery: {ps.get('masteryEffect', '?')} | "
                          f"Vers: {ps.get('versatility', '?')}")
            lines.append("")

    return "\n".join(lines)


def render_aura_findings(aura_records: list[dict]) -> str:
    """Render aura timeline findings."""
    if not aura_records:
        return ""

    lines = ["## Aura Timeline\n"]

    # Group by spellId, aggregate
    by_spell: dict[int, list[dict]] = {}
    for rec in aura_records:
        p = rec.get("p", {})
        spell_id = p.get("spellId")
        if spell_id is not None:
            by_spell.setdefault(spell_id, []).append(p)

    lines.append("| Aura | ID | Applications | Avg Duration | Max Stacks | Total Refreshes |")
    lines.append("|---|---|---|---|---|---|")

    for spell_id, entries in sorted(by_spell.items()):
        name = entries[0].get("spellName", "?")
        count = len(entries)
        durations = [e.get("duration", 0) for e in entries if e.get("duration")]
        avg_dur = sum(durations) / len(durations) if durations else 0
        max_stacks = max((e.get("maxStacks", 1) for e in entries), default=1)
        total_refreshes = sum(e.get("refreshCount", 0) for e in entries)

        lines.append(
            f"| {name} | {spell_id} | {count} | {avg_dur:.1f}s | {max_stacks} | {total_refreshes} |"
        )

    lines.append("")
    return "\n".join(lines)


def render_anomalies(anomaly_records: list[dict]) -> str:
    """Render talent anomalies if any."""
    if not anomaly_records:
        return ""

    lines = ["## Talent Anomalies\n"]
    for rec in anomaly_records:
        p = rec.get("p", {})
        lines.append(f"- **Spell {p.get('spellId', '?')}** ({p.get('spellName', '?')}): "
                      f"{p.get('description', 'no description')}")
    lines.append("")
    return "\n".join(lines)


def render_conclusion(
    timeouts: list[dict],
    fulfilled: list[dict],
    stats_records: list[dict],
    anomalies: list[dict],
) -> str:
    """Render the summary conclusion section."""
    lines = ["## Conclusion\n"]

    # Categorize
    high_conf_broken = [r for r in timeouts if r.get("p", {}).get("confidence") == "high"]
    med_conf_broken = [r for r in timeouts if r.get("p", {}).get("confidence") != "high"]

    if high_conf_broken:
        lines.append("### Likely Broken\n")
        lines.append("These deterministic procs never fired despite sufficient triggers:\n")
        for rec in high_conf_broken:
            p = rec.get("p", {})
            lines.append(f"- Spell {p.get('triggerSpellId')} -> {p.get('expectedSpellId')}: "
                          f"{p.get('notes', 'no notes')}")
        lines.append("")

    if med_conf_broken:
        lines.append("### Needs More Samples\n")
        lines.append("These probabilistic procs timed out but may need longer testing sessions "
                      "to confirm. Low expected chance means more triggers are needed for statistical "
                      "significance.\n")
        for rec in med_conf_broken:
            p = rec.get("p", {})
            lines.append(f"- Spell {p.get('triggerSpellId')} -> {p.get('expectedSpellId')} "
                          f"({p.get('expectedChance', 0)*100:.0f}% chance): "
                          f"{p.get('notes', 'no notes')}")
        lines.append("")

    if fulfilled and not timeouts:
        lines.append("### No Issue Observed\n")
        lines.append("All configured proc expectations were fulfilled during this audit session. "
                      "This does not guarantee correctness for all spells — only those with "
                      "configured expectations were monitored.\n")

    if not timeouts and not fulfilled:
        lines.append("### Insufficient Data\n")
        lines.append("No proc expectations were triggered during this session. This usually means "
                      "the session was too short, the class/spec lacks configured expectations, "
                      "or the relevant spells were not cast.\n")

    if anomalies:
        lines.append(f"### Anomalies: {len(anomalies)} detected\n")
        lines.append("See Talent Anomalies section above for details.\n")

    return "\n".join(lines)


def generate_report(
    loader: SavedVariablesLoader,
    session_id: int | None = None,
) -> str:
    """Generate the full Markdown audit report."""
    records = extract_audit_records(loader, session_id)
    if not records:
        return "# VoxSniffer Combat Audit Report\n\nNo CombatAudit records found.\n"

    groups = group_by_obs_type(records)

    summaries = groups.get("audit_summary", [])
    summary = summaries[-1] if summaries else None

    snapshots = groups.get("talent_snapshot", [])
    snapshot = snapshots[-1] if snapshots else None

    timeouts = groups.get("proc_timeout", [])
    fulfilled = groups.get("proc_fulfilled", [])
    stats_recs = groups.get("spell_stats", [])
    aura_recs = groups.get("aura_timeline", [])
    anomalies = groups.get("talent_anomaly", [])

    # Build report
    parts = []
    parts.append("# VoxSniffer Combat Audit Report\n")
    parts.append(f"*Generated {datetime.now().strftime('%Y-%m-%d %H:%M')}*\n")
    parts.append(render_session_metadata(summary, snapshot, len(records)))
    parts.append(render_proc_timeouts(timeouts))
    parts.append(render_proc_fulfilled(fulfilled))
    parts.append(render_spell_stats(stats_recs))
    parts.append(render_aura_findings(aura_recs))
    parts.append(render_anomalies(anomalies))
    parts.append(render_conclusion(timeouts, fulfilled, stats_recs, anomalies))

    return "\n".join(part for part in parts if part)


def main():
    parser = argparse.ArgumentParser(
        description="Generate a combat audit Markdown report from VoxSniffer SavedVariables"
    )
    parser.add_argument(
        "--input", "-i", required=True,
        help="Path to VoxSniffer.lua SavedVariables file"
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Output Markdown file path (default: stdout)"
    )
    parser.add_argument(
        "--session", "-s", type=int, default=None,
        help="Filter to specific VoxSniffer session ID"
    )
    args = parser.parse_args()

    loader = SavedVariablesLoader(args.input)
    try:
        loader.load()
    except (FileNotFoundError, SchemaError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    warnings = loader.validate()
    for w in warnings:
        print(f"Warning: {w}", file=sys.stderr)

    report = generate_report(loader, session_id=args.session)

    if args.output:
        out_path = Path(args.output)
        out_path.write_text(report, encoding="utf-8")
        print(f"Report written to {out_path}", file=sys.stderr)
    else:
        print(report)


if __name__ == "__main__":
    main()
