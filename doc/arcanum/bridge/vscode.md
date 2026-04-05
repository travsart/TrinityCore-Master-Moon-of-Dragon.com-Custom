---
description: "VS Code bridge — IDE transport types, permission relay to user, state synchronization, capability exchange, extension protocol"
---

# VS Code Bridge Protocol -- Arcanum Wiki

## Overview

Claude Code integrates with VS Code (and other IDEs) through MCP transport types (`sse-ide`, `ws-ide`) and a bridge protocol that enables state sync, permission delegation, and tool execution within the IDE context. The bridge connects the terminal-based Claude Code session with the IDE's extension, sharing diagnostics, file state, and execution capabilities.

## How It Works

### IDE Transport Types

Two transport types are specifically designed for IDE integration:

**`sse-ide`**: Server-Sent Events transport connected to the IDE extension's local SSE endpoint. Configured with `url`, `ideName`, and optional `ideRunningInWindows` flag.

**`ws-ide`**: WebSocket transport for bidirectional communication. Adds `authToken` for secure connections.

### IDE Tool Filtering

Only two tools from IDE MCP servers are exposed to the model:

```typescript
const ALLOWED_IDE_TOOLS = ['mcp__ide__executeCode', 'mcp__ide__getDiagnostics']
```

All other IDE-provided tools are filtered out to prevent the model from accessing IDE internals that could cause confusion or security issues.

### State Sync

The bridge maintains synchronized state between the Claude Code session and the IDE:
- File change notifications flow from the IDE to Claude Code
- Permission decisions can be displayed in the IDE's UI
- Diagnostic information (errors, warnings) from the IDE's language servers is accessible via `getDiagnostics`
- Code execution requests can be routed through the IDE's runtime

### Bridge Configuration

The bridge connection is configured via `bridgeConfig.ts` and `envLessBridgeConfig.ts`. Configuration can come from:
- IDE extension settings (passed at connection time)
- Environment variables (for headless/CI scenarios)
- CLI flags (`--chrome`, `--no-chrome`)

### Poll-Based Updates

`pollConfig.ts` manages periodic polling for bridge state changes, with configurable intervals defined in `pollConfigDefaults.ts`. This handles scenarios where the IDE extension and Claude Code session need to stay synchronized without persistent connections.

### Inbound Messages

`inboundMessages.ts` and `inboundAttachments.ts` process messages and file attachments arriving from the IDE. These are translated into the Claude Code message format and injected into the conversation at appropriate points.

### Bridge UI

`bridgeUI.ts` manages the terminal-side representation of bridge status, including connection state, remote client identification, and activity indicators. `BridgeDialog.tsx` provides the interactive connection setup UI.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/bridge/initReplBridge.ts` | Bridge initialization during REPL startup |
| `src/bridge/bridgeConfig.ts` | Bridge configuration management |
| `src/bridge/bridgeMessaging.ts` | Message routing between bridge and REPL |
| `src/bridge/bridgeUI.ts` | Terminal-side bridge status display |
| `src/bridge/bridgePermissionCallbacks.ts` | Permission relay to IDE |
| `src/bridge/inboundMessages.ts` | IDE-to-Claude message processing |
| `src/bridge/inboundAttachments.ts` | File attachment handling |
| `src/bridge/replBridgeTransport.ts` | Transport layer abstraction |
| `src/bridge/flushGate.ts` | Message ordering guarantees |

## Configuration

The IDE integration is typically configured through the IDE extension's settings, which are passed to Claude Code via the bridge protocol. Manual configuration is possible via:
- `.mcp.json` with `sse-ide` or `ws-ide` server configs
- `CLAUDE_CODE_REMOTE` environment variable
- CLI flags for Chrome extension integration

## Cross-References

- [Bridge Overview](overview.md) -- General bridge architecture
- [MCP Transports](../mcp/transports.md) -- IDE transport types
- [Permissions Overview](../permissions/overview.md) -- How bridge participates in permission decisions

## Interesting Findings

**The bridge is bidirectional but asymmetric.** Claude Code sends tool results and permission requests to the IDE; the IDE sends messages, file changes, and permission decisions back. The IDE never directly invokes tools -- it sends user intent which Claude Code processes through its normal pipeline.

**Flush gate ensures ordering.** `flushGate.ts` implements a mechanism to ensure messages are processed in order, preventing race conditions when multiple state updates arrive simultaneously from the IDE.

**JWT-based authentication.** `jwtUtils.ts` handles token management for the bridge connection, including automatic refresh scheduling to prevent session expiry during long-running conversations.
