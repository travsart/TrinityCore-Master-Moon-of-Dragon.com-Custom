---
description: "bridge architecture — IDE integration protocol, WebSocket communication, state synchronization, capability negotiation, transport layer"
---

# Bridge Architecture
> Source: `src/bridge/` (31 files)
> Status: STUB — needs research

## What This Covers
The bridge layer connects Claude Code CLI to IDE integrations (VS Code extension, Desktop app). Handles session management, permission callbacks, and WebSocket communication.

## Source Files to Read
- `src/bridge/` — all 31 files
- Focus: session lifecycle, WebSocket protocol, permission forwarding

## Key Questions
- How does the VS Code extension communicate with the CLI process?
- What protocol does the WebSocket use? (JSON-RPC? Custom?)
- How are permissions forwarded from IDE UI to CLI?
- What session state transfers between bridge and CLI?
