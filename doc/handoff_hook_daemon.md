# Handoff: Hook Dispatcher Daemon — IMPLEMENTED (2026-04-08)

**Status**: DONE. Daemon is live, 18/20 hooks migrated to `type: "http"`, 2 remain as command-type for SessionStart constraints. See "What shipped" below.

**Original plan**: `C:/Users/atayl/.claude/plans/imperative-drifting-hoare.md`

---

## Why the original TCP+dispatch.py architecture was wrong

The first draft of this handoff proposed wrapping each hook in `python dispatch.py` that talked to a raw TCP daemon. **That approach doesn't solve the problem** — it still spawns a Python subprocess per hook (for `dispatch.py`), so the ~100ms Windows Python cold-start cost is merely moved, not eliminated.

The 2.1.97 source refresh revealed the correct solution: **Claude Code natively supports `type: "http"` hooks**. CC POSTs the event JSON directly to a configured URL and parses the response body the same way it parses command stdout. Zero subprocess. Source evidence:

- `v2.1.88 src/utils/hooks.ts:1850-1864` — HTTP hooks work for all events except `SessionStart` and `Setup` (sandbox ask callback deadlock).
- `v2.1.88 src/utils/hooks/execHttpHook.ts:201-217` — CC uses `axios.post` with `Content-Type: application/json` and `validateStatus: () => true`. SSRF guard allows loopback.
- `v2.1.88 src/utils/hooks.ts:453-486` — `parseHttpHookOutput` accepts empty body (pass-through), `{"decision": "block", "reason": "..."}`, and `{"hookSpecificOutput": {"hookEventName": ..., "permissionDecision": "allow|deny|ask|defer"}}`.

Result: 18/20 hooks run without spawning any subprocess. The daemon handles hot-path latency at ~1 ms per hook (measured).

---

## Architecture

```
Claude Code 2.1.97
  │
  ├─ PreToolUse(Bash)       ──► HTTP POST localhost:19484/hook/sql-safety           ──┐
  ├─ PreToolUse(Bash)       ──► HTTP POST localhost:19484/hook/release-gate-enforce ──┤
  ├─ PreToolUse(Edit|Write) ──► HTTP POST localhost:19484/hook/sensitive-file-guard ──┤
  ├─ PostToolUse(Write|Edit)──► HTTP POST localhost:19484/hook/cpp-build-reminder   ──┤
  ├─ PostToolUse(Edit)      ──► HTTP POST localhost:19484/hook/edit-verifier        ──┤
  ├─ PostToolUse(Write|Edit)──► HTTP POST localhost:19484/hook/release-gate-revalidate ──┤
  ├─ PostToolUse(Read)      ──► HTTP POST localhost:19484/hook/large-file-guard     ──┤
  ├─ PostToolUse(Bash)      ──► HTTP POST localhost:19484/hook/sync-on-git          ──┤
  ├─ PostToolUse(*)         ──► HTTP POST localhost:19484/hook/session-stats        ──┤
  ├─ UserPromptSubmit       ──► HTTP POST localhost:19484/hook/timestamp-injector   ──┤── ► hook_daemon.py
  ├─ UserPromptSubmit       ──► HTTP POST localhost:19484/hook/prompt-context-injector┤   (asyncio HTTP server,
  ├─ PreCompact             ──► HTTP POST localhost:19484/hook/precompact-snapshot  ──┤    stdlib only,
  ├─ SubagentStop           ──► HTTP POST localhost:19484/hook/subagent-complete    ──┤    127.0.0.1:19484,
  ├─ Notification           ──► HTTP POST localhost:19484/hook/notification-toast   ──┤    18 routes)
  ├─ Stop                   ──► HTTP POST localhost:19484/hook/stop-verify          ──┤
  ├─ Stop                   ──► HTTP POST localhost:19484/hook/session-stats        ──┤
  ├─ PostToolUseFailure(Read)──► HTTP POST localhost:19484/hook/docx-auto-extract   ──┤
  ├─ SessionEnd             ──► HTTP POST localhost:19484/hook/cowork-sync          ──┘
  │
  ├─ SessionStart           ──► command: python daemon_shim.py (~100ms, health-check + spawn daemon if dead, exits 0)
  └─ SessionStart(compact)  ──► command: python compact-reinject.py (legacy, reads snapshot)
```

**Key properties**:
- All hot-path hooks use `type: "http"`. Zero Python subprocess cold start during a turn.
- `daemon_shim.py` is the only command-type hook on SessionStart. Its sole job is "ensure daemon is running". It reads stdin (to not block CC), health-checks the daemon, spawns it detached if dead, exits 0 always.
- `compact-reinject.py` stays as command-type (SessionStart(compact)) because HTTP hooks are blocked for SessionStart events.
- All 18 HTTP handlers live inside `hook_daemon.py` as `async def handle_<name>(data: dict) -> dict` functions. No module directory.
- Route table maps URL path → handler. CC fires one POST per registered hook entry.
- Each handler is wrapped in try/except. On exception → log and return HTTP 200 `{}` (pass-through). Daemon stays up.
- Async/advisory handlers (`session-stats`, toasts, `sync-on-git`, `cowork-sync`) call `asyncio.create_task()` and return `{}` immediately so CC never blocks on background work.

---

## What shipped

### New files

| File | Purpose |
|------|---------|
| `.claude/hooks/hook_daemon.py` | The daemon — asyncio HTTP server, 18 handlers, lifecycle, logging. Stdlib only. |
| `.claude/hooks/daemon_shim.py` | SessionStart auto-starter. 70 lines. No hook logic. |
| `.claude/hooks/_test_daemon.py` | Throwaway test harness used during development. Safe to delete. |

### Modified files

| File | Change |
|------|--------|
| `.claude/settings.local.json` | 18 hook entries converted from `type: "command"` to `type: "http"`. Added `daemon_shim.py` as first SessionStart hook. `compact-reinject.py` preserved as SessionStart(compact). `SessionEnd` changed from `command: python cowork/sync_bridge.py --full` to HTTP `/hook/cowork-sync`. |
| `.claude/hooks/release-gate-enforce.py` | Refactored `sys.exit(2) + stderr` → `{"decision": "block"}` + `sys.exit(0)`. CC treats both patterns identically, and the new shape is required for HTTP hook reuse. |
| `tools/shortcuts/start_all.bat` | Added step 7: start hook daemon on port 19484. Renumbered existing steps 1-6 to 1-7. |
| `doc/handoff_hook_daemon.md` | This file — rewritten against the actual implementation. |

### Files kept as-is (fallback / legacy)

All legacy hook scripts remain on disk. They are no longer registered in `settings.local.json` (except for the 2 SessionStart scripts), but the logic in each one is lifted into the corresponding `handle_<name>` function in the daemon. **Keep them for rollback.** Once the daemon has been stable for a full week, they can be deleted.

---

## Daemon protocol

### Request (CC → daemon)

```
POST /hook/<handler-name> HTTP/1.1
Host: 127.0.0.1:19484
Content-Type: application/json
Content-Length: N

{
  "hook_event_name": "PreToolUse",
  "tool_name": "Bash",
  "tool_input": {"command": "..."},
  "session_id": "...",
  ... (all CC hook event fields)
}
```

### Response (daemon → CC)

Always HTTP 200 with a JSON body. The body conforms to CC's hook output schema:

| Response | Effect |
|---|---|
| `{}` | Pass-through. No action. Use for advisory/async hooks. |
| `{"decision": "block", "reason": "..."}` | Block the tool call. CC shows reason to the model. Used by `sql-safety`, `release-gate-enforce`, `sensitive-file-guard`, `edit-verifier`. |
| `{"systemMessage": "..."}` | Inject a system message into the transcript. Used by `cpp-build-reminder`, `stop-verify`, `release-gate-revalidate`, `precompact-snapshot`, `large-file-guard`, `docx-auto-extract`, `subagent-complete`. |
| `{"additionalContext": "..."}` | Inject context into the user prompt (UserPromptSubmit only). Used by `timestamp-injector`, `prompt-context-injector`, `file-changed-monitor` (coordination category). |
| `{"hookSpecificOutput": {"hookEventName": "<event>", "permissionDecision": "allow\|deny\|ask\|defer"}}` | Per-event permission control (PreToolUse only). `defer` is new in 2.1.89. Not currently used by any handler but supported by the protocol. |

The daemon NEVER returns HTTP 4xx/5xx for hook errors — on handler exception it logs and returns `{}`, keeping CC's flow intact. The daemon returns 404 only for unknown URL paths and 400 only for non-POST requests to hook paths.

### Health endpoint

```
GET /health
→ 200 {"status":"ok","pid":N,"uptime":N,"version":"1.0.0","in_flight":N}
```

### Shutdown endpoint (Windows-only workaround)

```
POST /shutdown
→ 200 {"status":"shutting down"}
```

Windows can't send SIGTERM to detached Python processes (Python on Windows only handles SIGINT on console-attached processes). The `/shutdown` endpoint sets the daemon's `SHUTDOWN_EVENT`, triggering the graceful drain path: wait up to 5s for in-flight requests, delete PID file, exit.

---

## Handler catalog

All handlers are defined in `hook_daemon.py` sections E (sync blocking) and F (async advisory).

| Handler | Route | Sync/Async | Returns on block | Notes |
|---|---|---|---|---|
| `handle_sql_safety` | `/hook/sql-safety` | sync | `{"decision":"block"}` | Blocks `DROP TABLE`, `TRUNCATE`, `DELETE` without WHERE in mysql commands |
| `handle_release_gate_enforce` | `/hook/release-gate-enforce` | sync | `{"decision":"block"}` | Blocks `git push --tags`, `gh release create` when gate ≠ PASS |
| `handle_sensitive_file_guard` | `/hook/sensitive-file-guard` | sync | `{"decision":"block"}` | Blocks writes to Case_Reference/ etc. inside VoxCore repo |
| `handle_cpp_build_reminder` | `/hook/cpp-build-reminder` | sync | — | `systemMessage` for .cpp/.h edits |
| `handle_edit_verifier` | `/hook/edit-verifier` | sync | `{"decision":"block"}` | Re-reads file, verifies new_string present. Advisory-only for .md/.txt/etc. |
| `handle_release_gate_revalidate` | `/hook/release-gate-revalidate` | sync | — | Sets gate STALE on publishable/ edits. Also writes status file. |
| `handle_large_file_guard` | `/hook/large-file-guard` | sync | — | `systemMessage` when Read consumes >3000 lines |
| `handle_sync_on_git` | `/hook/sync-on-git` | async | — | Spawns sync_bridge.py detached on git commit/push/merge |
| `handle_session_stats` | `/hook/session-stats` | async | — | Appends JSONL line. Fires on 5 events. |
| `handle_timestamp_injector` | `/hook/timestamp-injector` | sync | — | `additionalContext` with timestamp |
| `handle_prompt_context_injector` | `/hook/prompt-context-injector` | sync | — | `additionalContext` based on keyword matching |
| `handle_precompact_snapshot` | `/hook/precompact-snapshot` | sync | — | Writes `~/.claude/precompact-state.json` |
| `handle_subagent_complete` | `/hook/subagent-complete` | async | — | JSONL + BurntToast detached subprocess |
| `handle_notification_toast` | `/hook/notification-toast` | async | — | BurntToast/Forms fallback, detached subprocess |
| `handle_stop_verify` | `/hook/stop-verify` | sync | — | `systemMessage` with workflow reminders. Advisory. |
| `handle_docx_auto_extract` | `/hook/docx-auto-extract` | sync (lazy import) | — | Extracts .docx via python-docx on Read failure |
| `handle_file_changed_monitor` | `/hook/file-changed-monitor` | sync | — | `additionalContext` for coordination/sql/cpp changes. Registered in settings? No — available but not wired |
| `handle_cowork_sync` | `/hook/cowork-sync` | async | — | 60s `asyncio.wait_for` on external `sync_bridge.py --full` |

**Not yet wired in settings.local.json** (exist as handlers, no registration):
- `file-changed-monitor` — requires CC to emit `FileChanged` events, which needs the `fileWatcherPatterns` policy setting wired up. Deferred.
- `deadline-alert` — stays as command-type legacy script because it's SessionStart-only and changes rarely enough that the ~100ms command-type cost is acceptable. Currently not wired at all.

---

## Verified

End-to-end tests run during implementation:

1. ✅ Daemon parses (syntax OK, `--version` prints `1.0.0`)
2. ✅ Daemon starts (`python hook_daemon.py`) and binds port 19484
3. ✅ PID file written to `~/.claude/hook_daemon.pid`
4. ✅ Rotating log file at `~/.claude/hook_daemon.log`
5. ✅ `/health` returns JSON with status/pid/uptime/version/in_flight
6. ✅ `sql-safety` blocks `DROP TABLE` in mysql command, allows `SELECT`, allows non-mysql
7. ✅ `sensitive-file-guard` blocks Case_Reference path inside VoxCore, allows outside
8. ✅ `timestamp-injector` returns `additionalContext` with current timestamp
9. ✅ `session-stats` returns `{}` and writes JSONL entry (verified via tail)
10. ✅ `release-gate-enforce` blocks `git push --tags`, allows `git push origin master`
11. ✅ `cpp-build-reminder` returns `systemMessage` for .cpp files
12. ✅ `prompt-context-injector` returns SQL context for "write a SQL update", empty for "yes"
13. ✅ Unknown path returns HTTP 404 with error body
14. ✅ Malformed JSON body returns HTTP 200 `{}` (graceful pass-through)
15. ✅ `stop-verify` returns `systemMessage` with workflow reminders when shared files edited
16. ✅ `large-file-guard` returns `{}` for small files
17. ✅ 30 concurrent POSTs complete in ~200ms total (asyncio concurrency verified)
18. ✅ Sequential latency: avg 0.94ms per request (min 0.48ms, max 7.17ms)
19. ✅ `/shutdown` endpoint triggers graceful drain, deletes PID file, exits cleanly
20. ✅ `daemon_shim.py` detects daemon-down state and auto-spawns within 3s
21. ✅ Live CC session integration: after settings.local.json cutover, ConfigChange/PostToolUse/Stop events flow to daemon in real time (verified via `~/.claude/session-stats.jsonl` tail)

**Latency improvement measured**: 119ms legacy cold start → 0.94ms HTTP round-trip = **~127× faster** per hook. On a busy turn with 20 hook invocations, that's 2,380ms → ~19ms total.

---

## Operations

### Starting the daemon

Automatic paths:
1. **SessionStart** — `daemon_shim.py` runs on every CC session start and auto-spawns the daemon if down.
2. **start_all.bat step 7** — starts the daemon on dev session boot if port 19484 is free.
3. **Any HTTP hook call** — if the daemon is down when CC tries to POST a hook, the request fails. There is no per-request auto-spawn (that would add latency to the hot path). The SessionStart shim is the only auto-start.

Manual:
```bash
python .claude/hooks/hook_daemon.py            # foreground
python .claude/hooks/hook_daemon.py --health   # health probe, exit 0/1
python .claude/hooks/hook_daemon.py --version  # print version
```

### Stopping the daemon

```bash
# Graceful (preferred)
python -c "import http.client; c=http.client.HTTPConnection('127.0.0.1',19484); c.request('POST','/shutdown'); print(c.getresponse().status)"

# Force (if graceful fails)
taskkill //F //PID $(cat ~/.claude/hook_daemon.pid)
rm ~/.claude/hook_daemon.pid
```

### Logs

- `~/.claude/hook_daemon.log` — daemon infrastructure log (startup, shutdown, handler errors, unknown paths, JSON parse errors). 5 MB rotation, 3 backups.
- `~/.claude/session-stats.jsonl` — tool-use analytics (written by `handle_session_stats`).

---

## Rollback

If the daemon causes problems:

1. **Stop daemon**: POST `/shutdown` or `taskkill //F //PID <pid>`.
2. **Revert settings**: `git checkout .claude/settings.local.json` restores the command-type hooks.
3. **Revert release-gate-enforce**: `git checkout .claude/hooks/release-gate-enforce.py` restores the `exit(2)` pattern (optional — the new JSON block pattern also works with command-type hooks).
4. **Legacy scripts are untouched** — command-type hooks resume working immediately.

Rollback cost: 2 commands, no data loss, no session interruption.

---

## Future improvements (not blockers)

- Wire `file-changed-monitor` once CC exposes `FileChanged` event configuration
- Register `deadline-alert` as an HTTP handler on UserPromptSubmit (it's already hook-mode capable)
- Add `TaskCreated`/`TaskCompleted` handler stubs for 2.1.97 coordinator lifecycle events (currently unused by VoxCore)
- Add `PermissionDenied` handler (new in 2.1.89) to catch auto-mode classifier denials
- Consider a daemon-side rate limiter for the BurntToast path (rapid subagent completions spawn many detached PowerShell processes)
- Migrate `session-stats.jsonl` to a size-rotating log to prevent unbounded growth
- Add `/metrics` endpoint returning per-route request counts + latency percentiles for observability
