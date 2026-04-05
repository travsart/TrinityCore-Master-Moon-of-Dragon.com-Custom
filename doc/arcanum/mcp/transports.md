---
description: "MCP transports — 8 transport types stdio SSE streamable websocket HTTP docker npx uvx, IDE filtering, connection setup"
---

# MCP Transport Types -- Arcanum Wiki

## Overview

Claude Code supports 8 MCP transport types, from local process spawning (stdio) to remote HTTP streaming. Each transport has different configuration fields, connection semantics, and security implications. The `type` field on stdio configs is optional for backward compatibility.

## How It Works

### Transport Matrix

| Transport | Config Fields | Use Case |
|-----------|---------------|----------|
| `stdio` | `command`, `args[]`, `env{}` | Local process spawning (default) |
| `http` | `url`, `headers{}`, `headersHelper?`, `oauth?` | MCP Streamable HTTP (primary remote) |
| `sse` | `url`, `headers{}`, `headersHelper?`, `oauth?` | Legacy Server-Sent Events |
| `ws` | `url`, `headers{}`, `headersHelper?` | WebSocket transport |
| `sse-ide` | `url`, `ideName`, `ideRunningInWindows?` | IDE extension SSE (internal) |
| `ws-ide` | `url`, `ideName`, `authToken?`, `ideRunningInWindows?` | IDE extension WebSocket (internal) |
| `sdk` | `name` | In-process SDK transport |
| `claudeai-proxy` | `url`, `id` | Claude.ai connector proxy |

### Environment Variable Expansion

Before configs are used, `${VAR}` and `${VAR:-default}` syntax is expanded. Expansion applies to: `command`, `args`, `env` (stdio); `url`, `headers` (remote). IDE/SDK/claudeai-proxy types pass through unchanged.

### IDE Tool Filtering

Only two IDE tools are exposed to the model:

```typescript
const ALLOWED_IDE_TOOLS = ['mcp__ide__executeCode', 'mcp__ide__getDiagnostics']
```

### In-Process Servers

Two servers bypass stdio process spawning: Chrome MCP (avoids a ~325MB subprocess) and Computer Use MCP (build-gated behind `CHICAGO_MCP`). Both use `createLinkedTransportPair()`.

### Graceful Shutdown

Stdio servers receive: SIGINT -> wait 100ms -> SIGTERM -> wait 400ms -> SIGKILL, with a 600ms absolute failsafe.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/mcp/types.ts` | Zod schemas for all 8 transports |
| `src/services/mcp/client.ts` | Transport creation and connection |

## Cross-References

- [MCP Overview](overview.md) -- Architecture
- [MCP Server Lifecycle](server_lifecycle.md) -- Connection management

## Interesting Findings

**Windows npx detection.** The config parser warns about `npx` commands on Windows that need a `cmd /c` wrapper.

**Fetch timeout architecture.** `wrapFetchWithTimeout()` applies 60-second timeouts only to POST requests. GET requests (long-lived SSE streams) are excluded. Uses `setTimeout` instead of `AbortSignal.timeout()` to avoid 2.4KB per-request memory leaks in Bun.
