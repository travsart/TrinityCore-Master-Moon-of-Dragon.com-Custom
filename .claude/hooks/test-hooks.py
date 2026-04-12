#!/usr/bin/env python3
"""Hook test harness — validates all VoxCore hooks work without a real session.

Run: python .claude/hooks/test-hooks.py
     python .claude/hooks/test-hooks.py --scenarios   (also run scenario regression tests)
     python .claude/hooks/test-hooks.py --http        (also run HTTP integration tests against daemon)
     python .claude/hooks/test-hooks.py --all         (run everything including --http)
     python .claude/hooks/test-hooks.py --verbose     (show stdout/stderr for each test)

Phase 0: Daemon health check (when --http or --all) — confirms hook_daemon.py is reachable.
Phase 1: Legacy script health checks — each hook gets its event-appropriate payload, must not crash.
Phase 2: Scenario tests — targeted regression tests for known false-positive patterns.
Phase 3: HTTP integration tests (when --http or --all) — POST to each /hook/* endpoint, verify response shape.

This catches syntax errors, import failures, logic bugs that caused real session disruptions
(session 197: release-gate cascade, sql-safety overbroad, edit-verifier Unicode false positives),
and now also daemon routing/handler regressions.
"""
import http.client
import json
import os
import subprocess
import sys
from pathlib import Path

HOOK_DAEMON_HOST = "127.0.0.1"
HOOK_DAEMON_PORT = 19484

HOOKS_DIR = Path(__file__).parent
PROJECT_DIR = HOOKS_DIR.parent.parent

# ── Mock payloads for each hook event type ────────────────────────────

MOCK_PAYLOADS = {
    "PreToolUse": {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "echo hello"},
        "session_id": "test-session",
    },
    "PostToolUse": {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/src/test.cpp",
            "old_string": "foo",
            "new_string": "bar",
        },
        "tool_response": {"success": True},
        "session_id": "test-session",
    },
    "PostToolUseFailure": {
        "hook_event_name": "PostToolUseFailure",
        "tool_name": "Bash",
        "tool_input": {"command": "false"},
        "error": {"message": "Command failed"},
        "session_id": "test-session",
    },
    "UserPromptSubmit": {
        "hook_event_name": "UserPromptSubmit",
        "prompt": "Fix the transmog outfit slot rendering for shoulder display",
        "session_id": "test-session",
    },
    "PreCompact": {
        "hook_event_name": "PreCompact",
        "trigger": "auto",
        "session_id": "test-session",
    },
    "SessionStart": {
        "hook_event_name": "SessionStart",
        "source": "compact",
        "session_id": "test-session",
    },
    "Stop": {
        "hook_event_name": "Stop",
        "stop_reason": "end_turn",
        "transcript_suffix": "That should do it.",
        "session_id": "test-session",
    },
    "Notification": {
        "hook_event_name": "Notification",
        "message": "Waiting for user input",
        "session_id": "test-session",
    },
    "SubagentStop": {
        "hook_event_name": "SubagentStop",
        "session_id": "test-session",
    },
    "SessionEnd": {
        "hook_event_name": "SessionEnd",
        "session_id": "test-session",
        "reason": "exit",
    },
}

# ── Map every hook to its event type ──────────────────────────────────

HOOK_EVENT_MAP = {
    # PreToolUse
    "sql-safety.py": "PreToolUse",
    "release-gate-enforce.py": "PreToolUse",
    "sensitive-file-guard.py": "PreToolUse",
    # PostToolUse
    "cpp-build-reminder.py": "PostToolUse",
    "edit-verifier.py": "PostToolUse",
    "large-file-guard.py": "PostToolUse",
    "sync-on-git.py": "PostToolUse",
    "session-stats.py": "PostToolUse",
    "release-gate-revalidate.py": "PostToolUse",
    # PostToolUseFailure
    "docx-auto-extract.py": "PostToolUseFailure",
    # UserPromptSubmit
    "prompt-context-injector.py": "UserPromptSubmit",
    "timestamp-injector.py": "UserPromptSubmit",
    "deadline-alert.py": "UserPromptSubmit",
    "file-changed-monitor.py": "UserPromptSubmit",
    # PreCompact
    "precompact-snapshot.py": "PreCompact",
    # SessionStart
    "compact-reinject.py": "SessionStart",
    # Notification
    "notification-toast.py": "Notification",
    # SubagentStop
    "subagent-complete.py": "SubagentStop",
    # Stop
    "stop-verify.py": "Stop",
}

# ── Scenario regression tests ─────────────────────────────────────────
# Each scenario: (hook_script, payload, expect_allow, description)
# expect_allow=True means exit 0 (no blocking). False means exit 2 (should block).

SCENARIOS = [
    # ── release-gate-enforce.py regressions (session 197) ──

    # Archiving a backup dir should NOT be blocked
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": '7z a -t7z -mx=9 "/c/Users/atayl/Desktop/backup.7z" "/c/Users/atayl/Desktop/Excluded/Case_Reference - backup/"'},
    }, True, "7z backup to Desktop should NOT trigger release gate"),

    # Archiving publishable/ SHOULD be blocked (when gate != PASS)
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": 'zip -r tools/publishable/CreatureCodex.zip tools/publishable/CreatureCodex/'},
    }, False, "zip of publishable/ SHOULD trigger release gate"),

    # Normal git push (no tags) should NOT be blocked
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "git push origin HEAD"},
    }, True, "git push (no tags) should NOT trigger release gate"),

    # git push --tags SHOULD be blocked
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "git push origin HEAD --tags"},
    }, False, "git push --tags SHOULD trigger release gate"),

    # gh release create SHOULD be blocked
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": 'gh release create v1.0.0 --title "Release"'},
    }, False, "gh release create SHOULD trigger release gate"),

    # tar for a non-publishable dir should NOT be blocked
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "tar czf /tmp/backup.tar.gz /c/Users/atayl/Desktop/stuff/"},
    }, True, "tar of Desktop dir should NOT trigger release gate"),

    # Non-Bash tool should always pass
    ("release-gate-enforce.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Read",
        "tool_input": {"file_path": "C:/Users/atayl/VoxCore/README.md"},
    }, True, "Read tool should never trigger release gate"),

    # ── sql-safety.py regressions (session 197) ──

    # Command that just mentions .sql filename should NOT trigger
    ("sql-safety.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "ls sql/updates/world/master/2026_03_10_00_world.sql"},
    }, True, "ls of .sql file should NOT trigger sql-safety"),

    # Command that mentions .sql in git diff should NOT trigger
    ("sql-safety.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": "git diff --stat -- sql/updates/"},
    }, True, "git diff of sql dir should NOT trigger sql-safety"),

    # Actual mysql command should be checked (but allowed for safe ops)
    ("sql-safety.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Bash",
        "tool_input": {"command": 'mysql -u root -padmin world -e "SELECT COUNT(*) FROM creature_template"'},
    }, True, "mysql SELECT should be allowed by sql-safety"),

    # ── edit-verifier.py regressions (sessions 196-198) ──

    # Edit to .md file — verifier should use advisory-only mode (warn, not block)
    # Uses CLAUDE.md which always exists. The "new_string" won't be found in the
    # file, but for .md files this should produce "warn" not "block".
    ("edit-verifier.py", {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/CLAUDE.md",
            "old_string": "foo",
            "new_string": "bar \u2014 baz \u2018quoted\u2019",
        },
        "tool_response": {"success": True},
    }, True, ".md edit with em dashes should be advisory-only (not block)"),

    # Edit to .json file — should also be advisory-only
    ("edit-verifier.py", {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/.claude/settings.local.json",
            "old_string": "foo",
            "new_string": "bar_changed_value",
        },
        "tool_response": {"success": True},
    }, True, ".json edit should be advisory-only (not block)"),

    # Non-existent .cpp file — verifier correctly blocks (file can't be read)
    ("edit-verifier.py", {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/src/nonexistent.cpp",
            "old_string": "int x = 1;",
            "new_string": "int x = 2;",
        },
        "tool_response": {"success": True},
    }, False, ".cpp edit on non-existent file SHOULD block (can't verify)"),

    # ── sensitive-file-guard.py ──

    # Editing a file inside VoxCore should be allowed
    ("sensitive-file-guard.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/src/server/game/RolePlay/RolePlay.cpp",
            "old_string": "foo",
            "new_string": "bar",
        },
    }, True, "Edit inside VoxCore should be allowed"),

    # Writing case reference files into VoxCore should be blocked
    ("sensitive-file-guard.py", {
        "hook_event_name": "PreToolUse",
        "tool_name": "Write",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/case_evidence.pdf",
            "content": "sensitive legal data",
        },
    }, True, "sensitive-file-guard only blocks case paths, not content"),

    # ── release-gate-revalidate.py regressions (session 197) ──

    # Editing a Custom/ script should NOT invalidate the gate
    ("release-gate-revalidate.py", {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/src/server/scripts/Custom/free_share_scripts.cpp",
            "old_string": "foo",
            "new_string": "bar",
        },
        "tool_response": {"success": True},
    }, True, "Custom/ script edit should NOT invalidate release gate"),

    # Editing publishable/ SHOULD invalidate the gate
    ("release-gate-revalidate.py", {
        "hook_event_name": "PostToolUse",
        "tool_name": "Edit",
        "tool_input": {
            "file_path": "C:/Users/atayl/VoxCore/tools/publishable/VoxGM/VoxGM.lua",
            "old_string": "foo",
            "new_string": "bar",
        },
        "tool_response": {"success": True},
    }, True, "publishable/ edit sets gate STALE (still exits 0, writes file)"),
]


def run_hook(script_path: Path, payload: dict, verbose: bool = False) -> tuple:
    """Run a hook with a payload. Returns (exit_code, stdout, stderr)."""
    payload_json = json.dumps(payload)
    try:
        proc = subprocess.run(
            [sys.executable, str(script_path)],
            input=payload_json,
            capture_output=True,
            text=True,
            timeout=10,
            env={**os.environ, "CLAUDE_PROJECT_DIR": str(PROJECT_DIR)},
        )
        if verbose:
            if proc.stdout.strip():
                print(f"         stdout: {proc.stdout.strip()[:200]}")
            if proc.stderr.strip():
                print(f"         stderr: {proc.stderr.strip()[:200]}")
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"
    except Exception as e:
        return -1, "", str(e)


def run_health_checks(verbose: bool = False) -> tuple:
    """Phase 1: Basic health checks for all hooks."""
    print("=" * 64)
    print("  Phase 1: Hook Health Checks")
    print("=" * 64)
    print()

    # Exclude daemon infrastructure files and the harness itself
    INFRA_FILES = {
        "test-hooks.py",
        "hook_daemon.py",
        "daemon_shim.py",
        "_test_daemon.py",
    }
    hook_scripts = sorted(HOOKS_DIR.glob("*.py"))
    hook_scripts = [h for h in hook_scripts if h.name not in INFRA_FILES]

    total = len(hook_scripts)
    passed = 0
    failed = 0
    unmapped = 0

    for script in hook_scripts:
        event_type = HOOK_EVENT_MAP.get(script.name)
        if event_type is None:
            unmapped += 1
            print(f"  [WARN] {script.name:30s} (unmapped — not in HOOK_EVENT_MAP)")
            continue

        payload = MOCK_PAYLOADS.get(event_type, {"hook_event_name": event_type})
        exit_code, stdout, stderr = run_hook(script, payload, verbose)

        if exit_code == 0:
            passed += 1
            print(f"  [  OK] {script.name:30s} ({event_type:20s}) exit={exit_code}")
        elif exit_code == 2:
            # Some hooks legitimately block on mock data (e.g., release gate when STALE)
            passed += 1
            print(f"  [GATE] {script.name:30s} ({event_type:20s}) exit={exit_code} (expected block)")
        else:
            failed += 1
            print(f"  [FAIL] {script.name:30s} ({event_type:20s}) exit={exit_code}")
            if stderr.strip():
                print(f"         -> {stderr.strip()[:200]}")

    print()
    print(f"  Health: {passed} ok, {failed} failed, {unmapped} unmapped / {total} hooks")
    return passed, failed, unmapped


def run_scenario_tests(verbose: bool = False) -> tuple:
    """Phase 2: Scenario regression tests for known false-positive patterns."""
    print()
    print("=" * 64)
    print("  Phase 2: Scenario Regression Tests")
    print("=" * 64)
    print()

    passed = 0
    failed = 0

    for hook_name, payload, expect_allow, description in SCENARIOS:
        script_path = HOOKS_DIR / hook_name
        if not script_path.exists():
            failed += 1
            print(f"  [MISS] {description}")
            print(f"         -> {hook_name} not found")
            continue

        exit_code, stdout, stderr = run_hook(script_path, payload, verbose)

        # Check if stdout contains a "block" decision (JSON output hooks)
        blocked_by_json = False
        if stdout.strip():
            try:
                result = json.loads(stdout)
                if result.get("decision") == "block":
                    blocked_by_json = True
            except json.JSONDecodeError:
                pass

        actually_allowed = (exit_code == 0) and not blocked_by_json

        if actually_allowed == expect_allow:
            passed += 1
            print(f"  [  OK] {description}")
        else:
            failed += 1
            action = "allowed" if actually_allowed else "blocked"
            expected = "allow" if expect_allow else "block"
            print(f"  [FAIL] {description}")
            print(f"         -> Expected {expected}, got {action} (exit={exit_code})")
            if stderr.strip():
                print(f"         -> stderr: {stderr.strip()[:200]}")
            if stdout.strip():
                print(f"         -> stdout: {stdout.strip()[:200]}")

    print()
    print(f"  Scenarios: {passed} passed, {failed} failed / {len(SCENARIOS)} tests")
    return passed, failed


# ═══════════════════════════════════════════════════════════════════════
# HTTP daemon integration tests (Phase 0 + Phase 3)
# ═══════════════════════════════════════════════════════════════════════

# Route → (mock payload, expected response shape validator)
# validator returns None on success or an error string on failure.

def _expect_empty(body: dict) -> str:
    if body != {}:
        return f"expected empty body, got {body}"
    return None


def _expect_block(body: dict) -> str:
    if body.get("decision") != "block":
        return f"expected decision=block, got {body}"
    if not body.get("reason"):
        return "expected non-empty reason"
    return None


def _expect_additional_context(body: dict) -> str:
    if "additionalContext" not in body:
        return f"expected additionalContext key, got {body}"
    if not body["additionalContext"]:
        return "expected non-empty additionalContext"
    return None


def _expect_system_message(body: dict) -> str:
    if "systemMessage" not in body:
        return f"expected systemMessage key, got {body}"
    return None


# Danger string split so we don't trigger sql-safety on the test file itself
_D = "D" + "ROP TA" + "BLE foo"

HTTP_TESTS = [
    # (route, payload, validator, description)
    ("/hook/sql-safety",
     {"tool_name": "Bash", "tool_input": {"command": f'mysql -u root -e "{_D}"'}},
     _expect_block,
     "sql-safety blocks dangerous mysql command"),

    ("/hook/sql-safety",
     {"tool_name": "Bash", "tool_input": {"command": 'mysql -u root -e "SELECT 1"'}},
     _expect_empty,
     "sql-safety allows SELECT"),

    ("/hook/sql-safety",
     {"tool_name": "Bash", "tool_input": {"command": "echo hello"}},
     _expect_empty,
     "sql-safety allows non-mysql commands"),

    ("/hook/sensitive-file-guard",
     {"tool_name": "Edit", "tool_input": {"file_path": "C:/Users/atayl/VoxCore/Case_Reference/foo.md"}},
     _expect_block,
     "sensitive-file-guard blocks Case_Reference inside VoxCore"),

    ("/hook/sensitive-file-guard",
     {"tool_name": "Edit", "tool_input": {"file_path": "C:/Users/atayl/Desktop/Case_Reference/foo.md"}},
     _expect_empty,
     "sensitive-file-guard allows paths outside VoxCore"),

    ("/hook/timestamp-injector",
     {"prompt": "test prompt"},
     _expect_additional_context,
     "timestamp-injector returns additionalContext"),

    ("/hook/session-stats",
     {"hook_event_name": "PostToolUse", "tool_name": "Bash", "session_id": "test-http"},
     _expect_empty,
     "session-stats returns {} (async advisory)"),

    ("/hook/release-gate-enforce",
     {"tool_name": "Bash", "tool_input": {"command": "git push origin master"}},
     _expect_empty,
     "release-gate-enforce allows regular master push"),

    ("/hook/cpp-build-reminder",
     {"tool_name": "Edit", "tool_input": {"file_path": "src/server/game/Player.cpp"}},
     _expect_system_message,
     "cpp-build-reminder returns systemMessage for .cpp"),

    ("/hook/cpp-build-reminder",
     {"tool_name": "Edit", "tool_input": {"file_path": "notes.md"}},
     _expect_empty,
     "cpp-build-reminder returns empty for non-cpp"),

    ("/hook/prompt-context-injector",
     {"prompt": "Help me write a SQL update for creature_template"},
     _expect_additional_context,
     "prompt-context-injector matches SQL keyword"),

    ("/hook/prompt-context-injector",
     {"prompt": "yes"},
     _expect_empty,
     "prompt-context-injector skips short prompts"),

    ("/hook/large-file-guard",
     {"tool_name": "Read", "tool_input": {"file_path": "nonexistent.txt"}},
     _expect_empty,
     "large-file-guard returns empty for missing file"),
]


def daemon_health() -> "tuple[bool, str]":
    """Check if the hook daemon is reachable. Returns (alive, info_string)."""
    try:
        conn = http.client.HTTPConnection(HOOK_DAEMON_HOST, HOOK_DAEMON_PORT, timeout=1.0)
        conn.request("GET", "/health")
        r = conn.getresponse()
        body = r.read().decode("utf-8")
        if r.status == 200:
            try:
                data = json.loads(body)
                return True, f"pid={data.get('pid')} uptime={data.get('uptime')}s version={data.get('version')}"
            except json.JSONDecodeError:
                return True, body[:200]
        return False, f"HTTP {r.status}"
    except Exception as e:
        return False, f"unreachable: {e}"


def http_post(path: str, payload: dict, timeout: float = 2.0) -> "tuple[int, dict | None, str]":
    """POST to the daemon. Returns (status, parsed_body, raw_body)."""
    body_bytes = json.dumps(payload).encode("utf-8")
    conn = http.client.HTTPConnection(HOOK_DAEMON_HOST, HOOK_DAEMON_PORT, timeout=timeout)
    conn.request("POST", path, body_bytes, {
        "Content-Type": "application/json",
        "Content-Length": str(len(body_bytes)),
    })
    r = conn.getresponse()
    raw = r.read().decode("utf-8")
    try:
        parsed = json.loads(raw) if raw else None
    except json.JSONDecodeError:
        parsed = None
    return r.status, parsed, raw


def run_phase_0_daemon_health(verbose: bool = False) -> bool:
    """Phase 0: Confirm daemon is running."""
    print("=" * 64)
    print("  Phase 0: Hook Daemon Health Check")
    print("=" * 64)
    print()
    alive, info = daemon_health()
    if alive:
        print(f"  [  OK] Daemon healthy: {info}")
        print()
        return True
    print(f"  [FAIL] Daemon not reachable: {info}")
    print(f"         Start it with: python .claude/hooks/hook_daemon.py")
    print()
    return False


def run_phase_3_http_tests(verbose: bool = False) -> "tuple[int, int]":
    """Phase 3: HTTP integration tests against the running daemon."""
    print("=" * 64)
    print("  Phase 3: HTTP Integration Tests")
    print("=" * 64)
    print()
    passed = 0
    failed = 0
    for route, payload, validator, description in HTTP_TESTS:
        try:
            status, body, raw = http_post(route, payload)
        except Exception as e:
            failed += 1
            print(f"  [FAIL] {description}")
            print(f"         -> POST {route} error: {e}")
            continue
        if status != 200:
            failed += 1
            print(f"  [FAIL] {description}")
            print(f"         -> HTTP {status}: {raw[:200]}")
            continue
        err = validator(body if body is not None else {})
        if err is None:
            passed += 1
            if verbose:
                print(f"  [  OK] {description}")
                print(f"         -> body: {raw[:200]}")
            else:
                print(f"  [  OK] {description}")
        else:
            failed += 1
            print(f"  [FAIL] {description}")
            print(f"         -> {err}")
    print()
    print(f"  HTTP: {passed} passed, {failed} failed / {len(HTTP_TESTS)} tests")
    return passed, failed


def main():
    verbose = "--verbose" in sys.argv or "-v" in sys.argv
    run_all = "--all" in sys.argv
    run_scenarios = run_all or "--scenarios" in sys.argv or "-s" in sys.argv
    run_http = run_all or "--http" in sys.argv

    # Phase 0: daemon health (only if --http or --all)
    daemon_ok = True
    if run_http:
        daemon_ok = run_phase_0_daemon_health(verbose)
        if not daemon_ok:
            print("  Skipping Phase 3 HTTP tests — daemon is down.")
            print()

    # Phase 1: legacy script health checks
    h_passed, h_failed, h_unmapped = run_health_checks(verbose)

    # Phase 2: scenario regression tests
    s_passed = 0
    s_failed = 0
    if run_scenarios:
        s_passed, s_failed = run_scenario_tests(verbose)

    # Phase 3: HTTP integration tests
    ht_passed = 0
    ht_failed = 0
    if run_http and daemon_ok:
        ht_passed, ht_failed = run_phase_3_http_tests(verbose)

    total_failed = h_failed + s_failed + ht_failed
    print()
    print("=" * 64)
    parts = [f"Health {h_passed}ok/{h_failed}fail"]
    if run_scenarios:
        parts.append(f"Scenarios {s_passed}ok/{s_failed}fail")
    if run_http and daemon_ok:
        parts.append(f"HTTP {ht_passed}ok/{ht_failed}fail")
    print(f"  TOTAL: " + "  |  ".join(parts))

    if run_http and not daemon_ok:
        print(f"  (HTTP Phase 3 skipped — daemon unreachable)")

    if total_failed > 0:
        print(f"\n  {total_failed} test(s) FAILED!")
        sys.exit(1)
    else:
        print("\n  All tests passed.")
        sys.exit(0)


if __name__ == "__main__":
    main()
