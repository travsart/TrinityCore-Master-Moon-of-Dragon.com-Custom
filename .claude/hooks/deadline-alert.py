#!/usr/bin/env python3
"""SessionStart / standalone deadline alert.

Reads .claude/deadlines.json and prints any deadline with <14 days remaining
(or marked severity HARD with <30 days) to stdout. Called by session-start rule
and can also be invoked as a UserPromptSubmit additionalContext source.

Two modes:
1. Standalone: `python deadline-alert.py` — prints human-readable alerts
2. Hook: reads stdin JSON (Claude Code hook protocol), emits additionalContext
         JSON if any critical deadlines found

DESIGN DECISIONS:
- Single source of truth: .claude/deadlines.json (update there, not here)
- Thresholds: HARD <30d = alert, any category <14d = alert
- Fails silently if deadlines.json is missing or malformed
- Silent if nothing is critical (no noise on clean days)
"""
import json
import os
import sys
from datetime import date, datetime

PROJECT_DIR = os.environ.get("CLAUDE_PROJECT_DIR", "C:/Users/atayl/VoxCore")
DEADLINES_PATH = os.path.join(PROJECT_DIR, ".claude", "deadlines.json")


def compute_alerts():
    """Return a list of (days_left, severity, name, notes) tuples for critical items."""
    try:
        with open(DEADLINES_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []

    today = date.today()
    alerts = []

    for item in data.get("deadlines", []):
        try:
            d = datetime.strptime(item["date"], "%Y-%m-%d").date()
        except (KeyError, ValueError):
            continue

        delta = (d - today).days
        severity = item.get("severity", "SOFT")

        # Alert thresholds
        is_critical = False
        if delta < 0:
            is_critical = True  # PAST DUE — always alert
        elif severity == "HARD" and delta < 30:
            is_critical = True
        elif delta < 14:
            is_critical = True

        if is_critical:
            alerts.append({
                "days": delta,
                "severity": severity,
                "category": item.get("category", ""),
                "name": item.get("name", "(unnamed)"),
                "date": item["date"],
                "owner": item.get("owner", ""),
                "notes": item.get("notes", ""),
            })

    # Sort by days ascending (most urgent first)
    alerts.sort(key=lambda a: a["days"])
    return alerts


def format_alerts_text(alerts):
    """Human-readable multi-line format."""
    if not alerts:
        return ""

    lines = ["!!! CRITICAL DEADLINES:"]
    for a in alerts:
        flag = "PAST DUE" if a["days"] < 0 else f"{a['days']}d"
        lines.append(f"  [{flag}] {a['category']} {a['severity']}: {a['name']} ({a['date']})")
        if a["owner"]:
            lines.append(f"      owner: {a['owner']}")
        if a["notes"]:
            lines.append(f"      {a['notes']}")
    return "\n".join(lines)


def main():
    alerts = compute_alerts()

    # Detect hook mode: if stdin has JSON, we're a hook; else standalone
    hook_mode = False
    stdin_data = {}
    if not sys.stdin.isatty():
        try:
            raw = sys.stdin.read()
            if raw.strip():
                stdin_data = json.loads(raw)
                hook_mode = True
        except (json.JSONDecodeError, ValueError):
            hook_mode = False

    if hook_mode:
        # UserPromptSubmit hook protocol: emit additionalContext JSON
        if alerts:
            text = format_alerts_text(alerts)
            result = {"additionalContext": text}
            json.dump(result, sys.stdout)
        sys.exit(0)

    # Standalone: print human-readable
    if alerts:
        print(format_alerts_text(alerts))
    else:
        print("No critical deadlines (nothing <14 days, no HARD <30 days, nothing past due).")


if __name__ == "__main__":
    main()
