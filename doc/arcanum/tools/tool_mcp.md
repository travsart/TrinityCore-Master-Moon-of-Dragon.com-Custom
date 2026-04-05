---
description: "MCP tools — MCPTool template, ListMcpResourcesTool, ReadMcpResourceTool, McpAuthTool, tool schema injection, passthrough parameters, server-specific tools"
---

# MCP Tools -- Arcanum Wiki

Covers `MCPTool`, `ListMcpResourcesTool`, `ReadMcpResourceTool`, and `McpAuthTool`.

## MCPTool

### Purpose
MCPTool is a template/base tool that gets cloned and customized for each MCP server tool at runtime. The base definition in `MCPTool.ts` is a skeleton -- most methods are overridden in `mcpClient.ts` when registering actual MCP tools.

### Parameters
The base schema accepts any object (`z.object({}).passthrough()`). Actual MCP tools have their own schemas defined by their MCP server, injected at registration time via `inputJSONSchema`.

### Key Implementation Details

The MCPTool source (src/tools/MCPTool/MCPTool.ts) is notably sparse at 77 lines because it's a template:

```typescript
export const MCPTool = buildTool({
  isMcp: true,
  // Overridden in mcpClient.ts with the real MCP tool name + args
  name: 'mcp',
  maxResultSizeChars: 100_000,
  // Overridden in mcpClient.ts
  async description() { return DESCRIPTION },
  async prompt() { return PROMPT },
  // Overridden in mcpClient.ts
  async call() { return { data: '' } },
  async checkPermissions(): Promise<PermissionResult> {
    return { behavior: 'passthrough', message: 'MCPTool requires permission.' }
  },
  // Overridden in mcpClient.ts
  userFacingName: () => 'mcp',
})
```

Four methods are explicitly marked "Overridden in mcpClient.ts": `name`, `description`, `prompt`, `call`, and `userFacingName`. The `isOpenWorld()` method is also overridden.

### Tool Naming Convention
MCP tools follow the naming pattern `mcp__<serverName>__<toolName>`. The `mcpInfo` property on each tool stores the original server and tool names as received from the MCP server (unnormalized).

### Permission Model
Base permission is `passthrough` -- each MCP tool gets its own permission rules based on its server and tool name.

### Result Truncation
The `isResultTruncated()` method delegates to `isOutputLineTruncated()`, providing consistent truncation detection for the expand-on-click UI.

---

## ListMcpResourcesTool

### Purpose
Lists available resources from connected MCP servers. Resources are server-provided data sources (files, databases, etc.) that can be read via `ReadMcpResourceTool`.

### Key Details
- Deferred tool (`shouldDefer: true`)
- Read-only, concurrency-safe
- Returns resource URIs, names, descriptions, and MIME types from all connected MCP servers

---

## ReadMcpResourceTool

### Purpose
Reads a specific resource from an MCP server by URI. Returns the resource content in the appropriate format (text, binary, etc.).

### Key Details
- Deferred tool (`shouldDefer: true`)
- Read-only, concurrency-safe
- Takes a `uri` parameter to identify the resource

---

## McpAuthTool

### Purpose
Handles OAuth/authentication flows for MCP servers that require user authentication. Manages the browser-based auth flow and token storage.

### Key Details
- Used when an MCP server returns an authentication-required error
- Launches browser for OAuth flow
- Stores tokens for subsequent requests

## Interesting Findings

1. The `isMcp: true` flag is unique to MCPTool and is checked throughout the codebase for MCP-specific handling (special permission paths, tool search categorization, etc.).

2. MCPTool's `maxResultSizeChars` of 100,000 applies to all MCP tools uniformly. Individual MCP servers cannot customize this threshold.

3. The MCPTool base has `isOpenWorld()` returning false by default, but this is overridden per-tool in mcpClient.ts based on the MCP server's configuration.

4. MCP tools can specify `alwaysLoad: true` via `_meta['anthropic/alwaysLoad']` in their MCP server configuration, overriding the default deferred loading behavior.
