---
description: "MCP server lifecycle — memoized connections, lazy initialization, reconnection logic, OAuth integration, graceful shutdown"
---

# MCP Server Lifecycle -- Arcanum Wiki

## Overview

MCP server connections are memoized by a composite key of server name plus serialized config. Connections are batched with different concurrency limits for local (3) and remote (20) servers. The system handles reconnection with exponential backoff, session expiry detection, needs-auth caching, and graceful shutdown with multi-signal escalation.

## How It Works

### Memoized Connection Cache

`connectToServer()` is memoized by `getServerCacheKey()` which combines `name` and `JSON.stringify(config)`. Repeated connection requests return the cached promise. The cache is cleared on `client.onclose`, triggering reconnection on next access.

### Batched Connection

Servers are partitioned into local (stdio/sdk) and remote (everything else) groups:
- Local batch: 3 concurrent connections (`MCP_SERVER_CONNECTION_BATCH_SIZE`)
- Remote batch: 20 concurrent connections (`MCP_REMOTE_SERVER_CONNECTION_BATCH_SIZE`)

Both groups run in parallel via `pMap()`.

### Connection Timeout

Default 30 seconds, overridable via `MCP_TIMEOUT` env var.

### Needs-Auth Fast Path

A file-backed auth cache (`mcp-needs-auth-cache.json`) with 15-minute TTL tracks servers that recently returned 401. On next connection attempt, these are immediately classified as `needs-auth` without a network round-trip.

### Reconnection

Managed by `useManageMCPConnections` React hook with exponential backoff:

```typescript
MAX_RECONNECT_ATTEMPTS = 5
INITIAL_BACKOFF_MS = 1000
MAX_BACKOFF_MS = 30000
```

For remote transports, consecutive terminal errors (ECONNRESET, ETIMEDOUT, EPIPE, ECONNREFUSED) are tracked. After 3 consecutive failures, `client.close()` is manually triggered.

### Session Expiry

HTTP/claudeai-proxy transports detect session expiry via HTTP 404 with JSON-RPC error code -32001. On detection: transport closed, connection cache cleared, tool call retried once.

### OAuth

The `ClaudeAuthProvider` implements PKCE with secure keychain storage, lock-file serialized refresh, and RFC 9728 discovery cascade. Token revocation follows RFC 7009 (refresh token first, then access token).

### Project Server Approval

`.mcp.json` project servers require approval: only servers with `getProjectMcpServerStatus() === 'approved'` are included. The `enableAllProjectMcpServers` setting auto-approves all project servers (trust boundary: anyone with repo write access).

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/mcp/client.ts` | Connection management, retry, session expiry |
| `src/services/mcp/useManageMCPConnections.ts` | React hook, reconnection backoff |
| `src/services/mcp/auth.ts` | OAuth provider, discovery, revocation |

## Cross-References

- [MCP Overview](overview.md) -- Architecture
- [MCP Transports](transports.md) -- Transport types

## Interesting Findings

**Stderr from stdio servers is accumulated.** Capped at 64MB, it is available for debugging failed connections.

**Deduplication prevents double-running.** Plugin servers with the same underlying command or URL as manual servers are suppressed. Between plugins, first-loaded wins. Claude.ai connectors whose URL matches a manual server are also suppressed.
