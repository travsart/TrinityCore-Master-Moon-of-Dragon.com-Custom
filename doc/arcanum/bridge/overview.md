---
description: "bridge overview — Remote Control architecture, work secrets authentication, session spawning, 31 source files, IDE integration"
---

# Bridge Architecture -- Arcanum Wiki

## Overview

The bridge system enables Claude Code sessions to be controlled from remote clients -- primarily the claude.ai web UI via "Remote Control" (`claude remote-control`). The architecture consists of a bridge main loop that registers an environment with Anthropic's API, polls for work items (sessions), spawns local Claude Code processes for each session, and relays state (permissions, tool results, messages) between the remote client and the local session via WebSocket.

## How It Works

### Bridge Main Loop

`bridgeMain.ts` implements the persistent bridge process:

1. **Register environment**: Call `registerBridgeEnvironment()` with machine name, branch, git URL, max sessions, spawn mode
2. **Poll for work**: Long-poll `pollForWork()` which returns session requests or healthchecks
3. **Spawn session**: For each work item, spawn a Claude Code child process with `--sdk-url` pointing to the session ingress
4. **Monitor**: Track session state, report activity, handle timeouts

The bridge supports three spawn modes:
- `single-session`: One session in CWD, bridge tears down when it ends
- `worktree`: Persistent server, every session gets an isolated git worktree
- `same-dir`: Persistent server, sessions share CWD (can stomp each other)

### Work Secret

Each session receives a `WorkSecret` (base64url-encoded JSON) containing:
- `session_ingress_token` -- Auth token for the WebSocket connection
- `api_base_url` -- API endpoint for the session
- `sources` -- Git info for source cloning (if applicable)
- `auth` -- Authentication tokens
- `claude_code_args` -- Additional CLI arguments
- `mcp_config` -- MCP server configuration to inject
- `environment_variables` -- Environment to set

### Session Spawning

`sessionRunner.ts` manages child process lifecycle:
- Spawns `claude` binary with `--sdk-url` flag
- Passes work secret via environment
- Monitors process health via exit codes
- Supports graceful shutdown (SIGTERM) with configurable grace period
- Reports session completion status: `completed`, `failed`, or `interrupted`

### REPL Bridge

`replBridge.ts` / `replBridgeHandle.ts` implement the in-session bridge that runs inside each spawned Claude Code process. This bridges the WebSocket transport to the local REPL:
- Receives user messages from the remote client
- Forwards tool outputs and assistant responses back
- Handles permission requests (sending them to the remote UI for approval)
- Manages state sync between local and remote representations

### Permission Relay

`bridgePermissionCallbacks.ts` implements the remote permission flow:
- When a permission prompt appears locally, it is forwarded to the remote client via the bridge
- The remote client can approve/deny/modify the request
- The response is relayed back to the local permission handler
- Part of the 4-way permission race (user local, bridge remote, channel, hooks+classifier)

### Backoff Configuration

```typescript
const DEFAULT_BACKOFF: BackoffConfig = {
  connInitialMs: 2_000,
  connCapMs: 120_000,      // 2 minutes
  connGiveUpMs: 600_000,   // 10 minutes
  generalInitialMs: 500,
  generalCapMs: 30_000,
  generalGiveUpMs: 600_000,
}
```

### Session Timeout

Default per-session timeout is 24 hours (`DEFAULT_SESSION_TIMEOUT_MS`). Sessions exceeding this are killed.

### Multi-Session Spawn

Gated behind `tengu_ccr_bridge_multi_session`. When enabled, the bridge can manage up to `maxSessions` (default 32) concurrent sessions.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/bridge/bridgeMain.ts` | Main bridge loop, environment registration, work polling |
| `src/bridge/sessionRunner.ts` | Child process spawning and management |
| `src/bridge/replBridge.ts` | In-session WebSocket bridge |
| `src/bridge/bridgeApi.ts` | API client for environment/work endpoints |
| `src/bridge/bridgePermissionCallbacks.ts` | Remote permission forwarding |
| `src/bridge/types.ts` | Bridge protocol types |
| `src/bridge/workSecret.ts` | Work secret encoding/decoding |

## Configuration

| Variable | Effect |
|----------|--------|
| `--spawn` / `--capacity` | Multi-session spawn modes |
| `--channels` | Channel servers for the session |
| `CLAUDE_CODE_REMOTE` | Indicates remote/cloud environment |
| Bridge config | `dir`, `machineName`, `branch`, `maxSessions`, `spawnMode`, `sandbox` |

## Cross-References

- [Bridge VS Code](vscode.md) -- VS Code-specific integration
- [Permissions Overview](../permissions/overview.md) -- 4-way permission race
- [Swarm Messaging](../agents/swarm_messaging.md) -- Related messaging patterns

## Interesting Findings

**Sleep detection in poll loop.** The bridge detects system sleep/wake by checking if the time between polls exceeds 2x the connection backoff cap. On wake, the error budget is reset to prevent accumulated backoff from blocking reconnection.

**Trusted device tokens.** `trustedDevice.ts` manages device-level authentication tokens that persist across sessions, reducing re-authentication frequency.

**CCR v2 compatibility.** The `use_code_sessions` field in WorkSecret indicates sessions created via the v2 compat layer, enabling different handling for legacy and modern session protocols.
