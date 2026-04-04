#!/usr/bin/env python3
"""FileChanged hook: detect external file modifications for multi-tab awareness.

Fires when another process (VS IDE, another Claude Code tab, etc.) modifies a file
in the VoxCore tree. Logs the change and injects context so Claude knows about it.

Hook event payload:
  - file_path: absolute path of the changed file
  - change_type: "modified" | "created" | "deleted"
  - hook_event_name: "FileChanged"
"""
import json
import sys
import os
from datetime import datetime, timezone
from pathlib import Path

LOG_FILE = os.path.expanduser("~/.claude/session-stats.jsonl")
VOXCORE_ROOT = os.environ.get("CLAUDE_PROJECT_DIR", r"C:\Users\atayl\VoxCore")

# File categories for targeted context injection
SQL_PATTERNS = (".sql",)
CPP_PATTERNS = (".cpp", ".h", ".hpp")
SESSION_FILES = ("session_state.md", "Central_Brain.md", "todo.md")


def categorize_change(file_path: str) -> str:
    """Return a human-readable category for the changed file."""
    name = os.path.basename(file_path)
    ext = os.path.splitext(file_path)[1].lower()

    if name in SESSION_FILES:
        return "coordination"
    if ext in SQL_PATTERNS:
        return "sql"
    if ext in CPP_PATTERNS:
        return "cpp"
    if "publishable" in file_path:
        return "addon"
    if "Case_Reference" in file_path:
        return "legal"
    return "other"


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    file_path = data.get("file_path", "")
    change_type = data.get("change_type", "modified")

    if not file_path:
        sys.exit(0)

    # Skip noise: .git internals, temp files, node_modules
    skip_patterns = (".git/", "node_modules/", "__pycache__/", ".tmp", "~")
    if any(p in file_path for p in skip_patterns):
        sys.exit(0)

    category = categorize_change(file_path)

    # Make path relative for readability
    try:
        rel_path = os.path.relpath(file_path, VOXCORE_ROOT)
    except ValueError:
        rel_path = file_path

    # Log to session stats
    entry = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "event": "FileChanged",
        "change_type": change_type,
        "file_path": rel_path,
        "category": category,
        "session": data.get("session_id", ""),
    }
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps(entry) + "\n")
    except Exception:
        pass

    # Inject context for coordination-critical files
    if category == "coordination":
        # These files are multi-tab coordination files — alert Claude
        print(json.dumps({
            "additionalContext": (
                f"[EXTERNAL CHANGE] {rel_path} was {change_type} by another process. "
                f"Re-read it before making decisions based on its content."
            )
        }))
    elif category == "sql" and "pending" in file_path.lower():
        print(json.dumps({
            "additionalContext": (
                f"[EXTERNAL CHANGE] SQL file {rel_path} was {change_type} externally. "
                f"Another tab may have applied or created SQL."
            )
        }))
    elif category == "cpp":
        print(json.dumps({
            "additionalContext": (
                f"[EXTERNAL CHANGE] C++ source {rel_path} was {change_type} externally. "
                f"A rebuild may be needed if you were planning to build."
            )
        }))


if __name__ == "__main__":
    main()
