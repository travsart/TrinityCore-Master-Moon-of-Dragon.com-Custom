#!/usr/bin/env python3
"""daemon_shim.py — ensure hook_daemon.py is running.

Called as a command-type hook on SessionStart (the only event that can't
use type:"http" because the sandbox ask callback deadlocks before
structuredInput is consumed). Fast path: health check via HTTP. Slow path:
spawn daemon detached if dead. Never blocks SessionStart — exits 0 even
on failure (a daemon failure must never break session start).

Plan: C:/Users/atayl/.claude/plans/imperative-drifting-hoare.md
"""

import http.client
import os
import subprocess
import sys
import time
from pathlib import Path

PORT = 19484
HOST = "127.0.0.1"
DAEMON = Path(__file__).resolve().parent / "hook_daemon.py"
LOG = Path.home() / ".claude" / "hook_daemon.log"


def daemon_alive() -> bool:
    try:
        conn = http.client.HTTPConnection(HOST, PORT, timeout=0.5)
        conn.request("GET", "/health")
        r = conn.getresponse()
        return r.status == 200
    except Exception:
        return False


def spawn_daemon() -> bool:
    DETACHED_PROCESS = 0x00000008
    CREATE_NEW_PROCESS_GROUP = 0x00000200
    LOG.parent.mkdir(parents=True, exist_ok=True)
    try:
        log_fp = open(LOG, "a", encoding="utf-8")
        subprocess.Popen(
            [sys.executable, str(DAEMON)],
            stdout=log_fp,
            stderr=log_fp,
            creationflags=DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
            close_fds=True,
        )
    except Exception:
        return False
    # Poll up to 3s for the server to bind and respond to /health
    for _ in range(30):
        time.sleep(0.1)
        if daemon_alive():
            return True
    return False


def main() -> int:
    # Read stdin if a hook payload was sent so we don't block CC waiting on us
    try:
        if not sys.stdin.isatty():
            sys.stdin.read()
    except Exception:
        pass
    if not daemon_alive():
        spawn_daemon()
    return 0


if __name__ == "__main__":
    sys.exit(main())
