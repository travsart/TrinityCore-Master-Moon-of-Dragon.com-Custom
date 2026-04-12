---
description: "LSP service — Language Server Protocol connection management, clangd typescript-language-server, server lifecycle, capabilities negotiation"
title: "LSP Service -- Arcanum Wiki"
tags: [services, server-lifecycle, capabilities-negotiation]
---

# LSP Service -- Arcanum Wiki

## What Is This?

The LSP (Language Server Protocol) service integrates language servers into Claude Code, providing type information, diagnostics, go-to-definition, and other IDE-like features. It manages a pool of language server instances, routes requests, and feeds diagnostic information back to the model.

## How It Works

### Architecture

The service has a layered architecture:

- **`manager.ts`** -- Global singleton manager. Handles initialization state (not-started, pending, success, failed) with generation counters to prevent stale promises from updating state.
- **`LSPServerManager.ts`** -- Manages multiple `LSPServerInstance` objects, one per language. Routes requests to the appropriate server based on file type.
- **`LSPServerInstance.ts`** -- Wraps a single language server process. Handles lifecycle (start, initialize, shutdown), capability negotiation, and request/response dispatch.
- **`LSPClient.ts`** -- Low-level JSON-RPC client that communicates with language server processes via stdin/stdout.
- **`LSPDiagnosticRegistry.ts`** -- Collects and deduplicates diagnostics (errors, warnings) from all connected servers.

### Initialization

`initializeLspServerManager()` is called during Claude Code startup. It:
1. Detects available language servers based on project file types
2. Starts server processes
3. Performs LSP `initialize` handshake with capability exchange
4. Registers notification handlers for diagnostics

The initialization is fully async -- tools check `getInitializationStatus()` and defer if still pending.

### Passive Feedback

`passiveFeedback.ts` registers handlers for LSP notifications (diagnostics, progress) that arrive without being requested. Diagnostics from language servers are collected and can be surfaced to the model as context about code health.

### Tool Integration

The LSP service backs the `LSPTool` which provides:
- Hover information (type info, documentation)
- Go-to-definition
- Find references
- Diagnostics for specific files

`isLspConnected()` checks whether at least one language server is healthy, used by `LSPTool.isEnabled()`.

### Configuration

`config.ts` contains language server configuration -- which servers to use for which languages, their startup commands, and initialization options.

## Key Source Files

| File | Purpose |
|------|---------|
| `manager.ts` | Global singleton, initialization state machine |
| `LSPServerManager.ts` | Multi-server management and request routing |
| `LSPServerInstance.ts` | Single server lifecycle and communication |
| `LSPClient.ts` | JSON-RPC transport layer |
| `LSPDiagnosticRegistry.ts` | Diagnostic collection and deduplication |
| `config.ts` | Language server configuration |
| `passiveFeedback.ts` | Notification handlers for diagnostics |

## Configuration

- Disabled in bare mode (`isBareMode()`)
- Auto-detected based on project file types
- `getInitializationStatus()` returns current state for deferred tool loading

## Interesting Findings

1. **Tools that depend on LSP use `shouldDeferLspTool()`** in `claude.ts` to check if LSP is still initializing. When LSP is pending, these tools are marked with `defer_loading: true` so the model knows they exist but cannot use them yet.

2. **The generation counter** prevents a race condition where an old initialization promise resolves after a reinitialize has started, which would overwrite the new state with stale results.

3. **The diagnostic registry deduplicates across servers** -- multiple language servers might report diagnostics for the same file if their scopes overlap.
