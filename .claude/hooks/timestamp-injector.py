#!/usr/bin/env python3
"""UserPromptSubmit hook: inject a timestamp into every user message.

Gives Claude temporal awareness — elapsed time between messages, time of day,
whether the user has been away. Always fires, no keyword filter.
"""
import json
import sys
from datetime import datetime


def main():
    try:
        data = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    now = datetime.now()
    timestamp = now.strftime("%Y-%m-%d %H:%M:%S")
    day_of_week = now.strftime("%A")

    result = {
        "additionalContext": f"[TIMESTAMP] User message received: {day_of_week} {timestamp}"
    }
    json.dump(result, sys.stdout)


if __name__ == "__main__":
    main()
