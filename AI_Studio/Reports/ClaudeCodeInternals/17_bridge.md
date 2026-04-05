# 17. Bridge (IDE Integration / Remote Control)

> Source: `src/bridge/` (31 files, ~15,000+ lines) + `src/commands/bridge/` (2 files)
> Feature flag: `feature('BRIDGE_MODE')` (compile-time) + `tengu_ccr_bridge` (GrowthBook runtime)
> Status: SHIPPED -- claude.ai subscribers, branded as "Remote Control"

## Executive Summary

The Bridge system (branded "Remote Control") connects a local Claude Code CLI process to Anthropic's cloud infrastructure (CCR -- Claude Code Runner) so users can interact with their coding sessions from claude.ai/code, the Claude mobile app, or IDE extensions. It has two operational modes (standalone bridge server, REPL bridge) and two protocol versions (v1 env-based with WebSocket, v2 env-less with SSE). The system handles session lifecycle, permission delegation, token refresh, crash recovery, and multi-session management.

**Key finding**: The bridge is NOT a WebSocket between VS Code and the CLI. It's a cloud-mediated system -- the CLI connects to Anthropic's CCR servers, and the web/mobile clients connect to the same servers. There is no direct IDE-to-CLI communication in the bridge layer.

## Architecture

```
┌──────────────────────┐        ┌──────────────────────┐
│  claude.ai/code      │        │  Claude Mobile App   │
│  (Web client)        │        │  (iOS/Android)       │
└──────────┬───────────┘        └──────────┬───────────┘
           │                               │
           ▼                               ▼
┌──────────────────────────────────────────────────────┐
│              Anthropic CCR Servers                    │
│  POST /v1/sessions    GET /work/poll                 │
│  POST /v1/environments/bridge                        │
│  WebSocket /session_ingress/ws/{id}  (v1)            │
│  SSE /worker/events/stream           (v2)            │
│  POST /worker/events                 (v2 writes)     │
└──────────────────────────────────────────────────────┘
           │                     │
           ▼                     ▼
┌────────────────────┐  ┌────────────────────┐
│  Standalone Bridge │  │    REPL Bridge     │
│  `claude remote-   │  │  (background in    │
│   control`         │  │   interactive      │
│  bridgeMain.ts     │  │   session)         │
│  (spawns children) │  │  replBridge.ts /   │
│                    │  │  remoteBridgeCore  │
└────────────────────┘  └────────────────────┘
```

## Two Operational Modes

### 1. Standalone Bridge (`claude remote-control`)

Entry: `bridgeMain.ts` (~2,400 lines). A persistent server that:
1. Registers an environment with CCR
2. Long-polls for work (session requests)
3. Spawns child `claude --print` processes per session
4. Manages concurrent sessions (default max: 32)
5. Routes permission requests between sessions and CCR
6. Handles worktree-per-session isolation

```
User runs: claude remote-control
  → registerBridgeEnvironment() → {environment_id, environment_secret}
  → Display QR code + connect URL
  → Enter poll loop:
     pollForWork() → WorkResponse → decodeWorkSecret()
     → spawn child process (claude --print --sdk-url <ws> --session-id <id>)
     → acknowledgeWork()
     → manage lifecycle until session ends
     → stopWork() + archiveSession()
  → On shutdown: deregisterEnvironment()
```

### 2. REPL Bridge (Always-On Background)

Entry: `initReplBridge.ts` → `replBridge.ts` (v1) or `remoteBridgeCore.ts` (v2). Runs inside an interactive REPL session:
1. Creates a session on CCR
2. Connects transport (WS or SSE)
3. Mirrors conversation events bidirectionally
4. Handles remote permission requests
5. Token refresh in background

The REPL bridge is activated via `/remote-control` command or `remoteControlAtStartup` config.

## Two Protocol Versions

### v1 (Environment-Based)

```
1. POST /v1/environments/bridge     → {environment_id, environment_secret}
2. POST /v1/sessions                → session_id
3. GET  /work/poll                  → WorkResponse (contains WorkSecret)
4. Decode WorkSecret                → {session_ingress_token, api_base_url}
5. Connect HybridTransport:
   - WebSocket wss://{host}/v1/session_ingress/ws/{session_id}  (reads)
   - HTTP POST to session_ingress URL                            (writes)
6. POST /work/{id}/ack
7. Stream messages bidirectionally
8. Cleanup: stopWork + archiveSession + deregisterEnvironment
```

### v2 (Env-Less / Direct)

```
1. POST /v1/code/sessions           → {id: 'cse_*'}
2. POST /v1/code/sessions/{id}/bridge → {worker_jwt, api_base_url, epoch}
3. Connect v2 transport:
   - SSETransport to /worker/events/stream  (reads)
   - CCRClient POST /worker/events          (writes)
4. Token refresh: re-call /bridge before expiry
5. Cleanup: archiveSession
```

v2 is gated by `tengu_bridge_repl_v2` GrowthBook flag.

## Transport Layer

### ReplBridgeTransport Interface

Unified abstraction over both protocol versions:

```typescript
type ReplBridgeTransport = {
  write(message: StdoutMessage): Promise<void>
  writeBatch(messages: StdoutMessage[]): Promise<void>
  close(): void
  isConnectedStatus(): boolean
  getStateLabel(): string
  setOnData(callback: (data: string) => void): void
  setOnClose(callback: (closeCode?: number) => void): void
  setOnConnect(callback: () => void): void
  connect(): void
  getLastSequenceNum(): number        // SSE resume position
  readonly droppedBatchCount: number  // write failure tracking
  reportState(state: SessionState): void
  reportMetadata(metadata): void
  reportDelivery(eventId, status): void
  flush(): Promise<void>
}
```

### v1 Transport (HybridTransport)

- WebSocket for reads (auto-reconnect, ping/pong, sleep detection)
- HTTP POST for writes via `SerialBatchEventUploader` (100ms buffer, max batch size 100)
- `wss://{host}/v1/session_ingress/ws/{sessionId}`

### v2 Transport (SSE + CCRClient)

- `SSETransport` for reads (reconnect budget, sequence-number resume via `Last-Event-ID`)
- `CCRClient` for writes (`POST /worker/events` via `SerialBatchEventUploader`)
- Heartbeat via `PUT /worker` at configurable interval (default 20s)
- Epoch-based worker identity (409 = superseded, triggers reconnect)

## Message Protocol (SDK Messages)

Messages flow as `SDKMessage` discriminated unions:

```
Outbound (CLI → CCR):
  { type: 'user',      message: {content}, uuid, session_id }
  { type: 'assistant',  message: {content}, uuid, session_id }
  { type: 'result',     subtype: 'success', duration_ms, num_turns, ... }
  { type: 'control_request',  request_id, request: {subtype: 'can_use_tool', ...} }
  { type: 'control_response', response: {subtype: 'success', request_id, ...} }

Inbound (CCR → CLI):
  { type: 'user',      message: {content}, uuid }     // remote user's prompt
  { type: 'control_response', response: {behavior: 'allow'|'deny', ...} }
  { type: 'control_request',  request: {subtype: 'initialize'|'set_model'|...} }
```

### Echo Deduplication

`BoundedUUIDSet` (ring buffer, 2000 entries) prevents:
- Self-echo: messages we sent being delivered back to us
- Re-delivery: SSE reconnects replaying already-processed events
- Flush-gate interleaving: initial history vs live message ordering

## Permission Delegation Flow

```
1. Claude wants to use a tool (e.g., Bash command)
2. interactiveHandler.ts checks if bridge callbacks exist
3. bridgeCallbacks.sendRequest(requestId, toolName, input, ...)
   → Sends control_request {subtype: 'can_use_tool'} through transport
4. Remote user on claude.ai sees permission prompt
5. User approves/denies
   → control_response {behavior: 'allow'|'deny', updatedPermissions?}
6. bridgeCallbacks.onResponse fires with decision
7. REPL allows/denies tool execution
8. If resolved locally (hooks/classifier), cancelRequest() dismisses remote prompt
```

```typescript
type BridgePermissionResponse = {
  behavior: 'allow' | 'deny'
  updatedInput?: Record<string, unknown>      // edited tool input
  updatedPermissions?: PermissionUpdate[]     // permission rule updates
  message?: string                            // denial reason
}
```

## Server Control Requests

The server can send control requests to the CLI:

| Subtype | Action |
|---------|--------|
| `initialize` | Returns `{commands, models, account, pid}` |
| `set_model` | Changes the active model |
| `set_max_thinking_tokens` | Adjusts thinking budget |
| `set_permission_mode` | Switches permission mode (plan, auto, etc.) |
| `interrupt` | Aborts current operation |

## Session Lifecycle Management

### Crash Recovery

`bridgePointer.json` persisted per-directory with session/environment IDs:
```typescript
{ sessionId: string, environmentId: string, source: 'standalone' | 'repl' }
```
- Written after session creation, refreshed periodically
- 4-hour TTL via mtime check
- `readBridgePointerAcrossWorktrees()` fans out for `--continue` support
- Cleared on clean shutdown

### Token Refresh

Two-layer refresh:
1. **Worker JWT** (from `/bridge`): refreshed proactively 5min before expiry
2. **OAuth token** (from keychain): refreshed via `withOAuthRetry()` on 401

### Session Timeout

Default 24 hours (`DEFAULT_SESSION_TIMEOUT_MS`). Configurable via `--session-timeout`. Watchdog timer kills exceeded sessions.

### Multi-Session Spawn Modes

| Mode | Behavior |
|------|----------|
| `single-session` | One session in cwd, bridge exits when it ends |
| `worktree` | Each session gets isolated git worktree |
| `same-dir` | Sessions share cwd (can stomp each other) |

Gated by `tengu_ccr_bridge_multi_session`. Default max sessions: 32.

## Feature Gating

### GrowthBook Flags

| Flag | Purpose |
|------|---------|
| `tengu_ccr_bridge` | Master enable for Remote Control |
| `tengu_bridge_repl_v2` | Enable v2 (env-less) bridge path |
| `tengu_bridge_repl_v2_config` | v2 timing configuration |
| `tengu_bridge_repl_v2_cse_shim_enabled` | Session ID retag shim |
| `tengu_bridge_min_version` | Minimum CLI version |
| `tengu_bridge_poll_interval_config` | Poll timing config |
| `tengu_bridge_initial_history_cap` | Max initial messages to flush |
| `tengu_cobalt_harbor` | Auto-connect on startup default |
| `tengu_ccr_mirror` | Outbound-only mirror mode |
| `tengu_ccr_bridge_multi_session` | Multi-session spawn modes |
| `tengu_sessions_elevated_auth_enforcement` | Trusted device tokens |

### Access Requirements

```typescript
function isBridgeEnabled(): boolean {
  return feature('BRIDGE_MODE')
    ? isClaudeAISubscriber() &&
        getFeatureValue('tengu_ccr_bridge', false)
    : false
}
```

Requires: claude.ai subscription + GrowthBook flag enabled. Excludes: Bedrock, Vertex, Foundry, API key users.

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `CLAUDE_BRIDGE_OAUTH_TOKEN` | Ant-only OAuth override |
| `CLAUDE_BRIDGE_BASE_URL` | Ant-only API base URL |
| `CLAUDE_BRIDGE_SESSION_INGRESS_URL` | Ant-only ingress URL |
| `CLAUDE_CODE_USE_CCR_V2` | Force CCR v2 transport |
| `CLAUDE_CODE_CCR_MIRROR` | Enable mirror mode |
| `CLAUDE_TRUSTED_DEVICE_TOKEN` | Trusted device token |
| `CLAUDE_CODE_SESSION_ACCESS_TOKEN` | Session token for children |

## IDE Detection (Separate from Bridge)

The bridge is NOT a direct IDE-to-CLI connection. IDE detection is a separate system:

```typescript
// src/utils/ide.ts -- detects via lockfiles and process trees
// Supports: VS Code, JetBrains (all 15+ IDEs), Cursor, Windsurf, Zed

// VS Code: lockfile at ~/.claude/ide/{port}.lock
// JetBrains: plugin directory scan for claude-code-jetbrains-plugin
// Others: process tree inspection
```

IDE extensions communicate via MCP protocol (not the bridge), while the bridge connects to claude.ai/code (the web app).

## Resilience Patterns

1. **Exponential backoff**: Connection errors: 2s initial, 120s cap, 600s give-up. General errors: 500ms initial, 30s cap, 600s give-up.
2. **Sleep/wake detection**: If poll takes >2x backoff cap, assume laptop slept, reset error budget.
3. **Heartbeat leases**: `POST /work/{id}/heartbeat` extends work item lease. Returns `{lease_extended, state}`.
4. **Graceful shutdown**: SIGTERM + 30s grace → SIGKILL cascade. Sessions killed in reverse order.
5. **Stale token backoff**: Cross-process dead-token detection via `BRIDGE_SKIP_UNTIL_MS` file.
6. **Transport swap**: v2 can rebuild transport on epoch mismatch (409) without losing the session.

## Actionable Findings

1. **We already have this** -- Remote Control is available to claude.ai subscribers. Run `claude remote-control` to start a standalone bridge, or `/remote-control` in an active session.

2. **The bridge is cloud-mediated** -- No direct IDE-to-CLI WebSocket. All traffic routes through Anthropic's CCR servers. This means it works across networks (e.g., mobile phone controlling laptop).

3. **Permission delegation is bidirectional** -- The remote user on claude.ai can approve/deny tool calls, change models, switch permission modes, and interrupt operations.

4. **Auto-connect exists** -- `tengu_cobalt_harbor` GrowthBook flag controls whether sessions auto-connect to CCR on startup. Currently ant-only.

5. **Mirror mode** -- `tengu_ccr_mirror` enables outbound-only event forwarding. The session mirrors to CCR but doesn't accept inbound prompts. Separate from full Remote Control.

6. **v2 is the future** -- The env-less path skips the entire register/poll/ack lifecycle, going directly from session creation to transport. Simpler, fewer API calls, but still behind a GrowthBook gate.

7. **31 files is a lot** -- This is by far the largest subsystem in Claude Code. The complexity reflects real-world challenges: two protocol versions, crash recovery, token refresh, multi-session management, permission delegation, and extensive error handling.
