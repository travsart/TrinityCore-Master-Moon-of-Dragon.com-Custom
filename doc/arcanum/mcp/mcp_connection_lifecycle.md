---
description: "MCP connection lifecycle — server discovery, spawn, initialize handshake, tool registration, reconnection, error handling"
---

# MCP Connection Lifecycle
> Source: `services/mcp/`, `utils/mcp/`
> Status: STUB — needs research

## What This Covers
How MCP servers connect, authenticate, discover tools, and handle disconnections.

## Known (from Tier 2)
- 7 config scopes, 8 transports
- Tool timeout ~27.8 hours (effectively infinite)
- 6-layer gating for channel push

## Key Questions
- Connection init sequence (config read → spawn → handshake → tool discovery)
- Transport types: stdio, SSE, WebSocket, HTTP — when is each used?
- Reconnection behavior on server crash
- How tool schemas are cached and refreshed
