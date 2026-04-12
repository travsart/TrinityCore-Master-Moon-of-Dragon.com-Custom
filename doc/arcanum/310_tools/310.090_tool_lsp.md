---
description: "LSPTool — Language Server Protocol, goToDefinition, findReferences, hover, documentSymbol, workspaceSymbol, callHierarchy, deferred tool, clangd integration"
title: "LSPTool -- Arcanum Wiki"
tags: [tools, language-server, gotodefinition, findreferences, hover, documentsymbol, workspacesymbol, callhierarchy]
---

# LSPTool -- Arcanum Wiki

## Purpose

LSPTool provides Language Server Protocol operations including go-to-definition, find-references, hover info, document symbols, workspace symbols, go-to-implementation, and call hierarchy (incoming/outgoing). It is deferred and only enabled when an LSP server is connected.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `operation` | enum | Yes | One of: `goToDefinition`, `findReferences`, `hover`, `documentSymbol`, `workspaceSymbol`, `goToImplementation`, `prepareCallHierarchy`, `incomingCalls`, `outgoingCalls` |
| `filePath` | string | Yes | Absolute or relative path to the file |
| `line` | number | Yes | Line number (1-based) |
| `character` | number | Yes | Character offset (1-based) |

## Execution Flow

1. **Validation**: Validates against a discriminated union schema for better error messages. Checks file exists, is a regular file, and under 10 MB.
2. **LSP initialization**: Waits for LSP server initialization if needed via `waitForInitialization()`.
3. **File opening**: Opens the file in the LSP server via `textDocument/didOpen`.
4. **Operation dispatch**: Routes to the appropriate LSP method (textDocument/definition, textDocument/references, etc.).
5. **Result formatting**: Formats results via operation-specific formatters (e.g., `formatGoToDefinitionResult`, `formatHoverResult`).

## Key Implementation Details

### File Size Cap
```typescript
const MAX_LSP_FILE_SIZE_BYTES = 10_000_000 // 10 MB
```

### Enablement Check
LSPTool is only enabled when `isLspConnected()` returns true. It is deferred (`shouldDefer: true`) so it doesn't appear in the initial tool schema for sessions without LSP support.

### 1-Based to 0-Based Conversion
LSP protocol uses 0-based line/character positions, but the tool accepts 1-based positions (as shown in editors). The conversion happens at the call site.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| MAX_LSP_FILE_SIZE_BYTES | 10,000,000 (10 MB) |
| `shouldDefer` | true |
| `isLsp` | true |
| Concurrency safe | Yes |
| Read only | Yes |

## Permission Requirements

Uses `checkReadPermissionForTool()`. Read-only and concurrency-safe.

## Interesting Findings

1. The `isLsp: true` flag on the tool is checked elsewhere in the codebase to identify LSP tools for special handling (e.g., deferred loading, connection-dependent enablement).

2. LSPTool has a dedicated `formatters.ts` module with eight different formatters, one per operation type, suggesting significant effort went into making LSP results readable for the model.
