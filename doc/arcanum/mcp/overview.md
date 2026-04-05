---
description: "MCP client overview — 7 config scopes, tool registration, server instructions, mcp__server__tool naming, 27.8h timeout, enableAllProjectMcpServers"
---

# MCP Client Architecture -- Arcanum Wiki

## Overview

The MCP (Model Context Protocol) subsystem connects Claude Code to external tool servers. It handles discovery of server configurations from 7 scopes, manages connections across 8 transport types, registers server-provided tools into the model's tool list, and orchestrates the full call/response cycle including OAuth, image resizing, binary persistence, and large-output offloading. The architecture uses memoized connections with LRU-cached tool fetches and batched concurrent connection management.

## How It Works

### Config Discovery Pipeline

```
getAllMcpConfigs()
  -> getClaudeCodeMcpConfigs()     (local/user/project/enterprise/dynamic/plugin)
  -> fetchClaudeAIMcpConfigs()     (claude.ai connectors -- lowest precedence)
  -> Policy filtering (allow/deny lists)
  -> getMcpToolsCommandsAndResources()
     -> Partition: local batch (3 concurrent) | remote batch (20 concurrent)
     -> connectToServer() per server (memoized by name+config hash)
     -> fetchToolsForClient() (LRU cached, size 20)
     -> Register tools into model context
```

### Tool Registration

Each MCP tool gets a fully qualified name: `mcp__<normalized_server_name>__<tool_name>`. Name normalization replaces non-alphanumeric characters with underscores. Tool descriptions are capped at 2,048 characters to prevent OpenAPI-generated servers from dumping massive docs into context.

### Tool Call Flow

```
Model selects mcp__server__tool
  -> ensureConnectedClient()
  -> callMCPToolWithUrlElicitationRetry()
     -> callMCPTool()
        -> client.callTool({ name, arguments, _meta })
        -> processMCPResult()
     -> handle -32042 UrlElicitation (up to 3 retries)
  -> handle McpSessionExpiredError (1 retry with fresh client)
```

Default tool call timeout: ~27.8 hours (`100_000_000ms`). Per-HTTP-request timeout: 60 seconds (POST only; long-lived SSE GETs are exempt).

### Connection States

| State | Meaning |
|-------|---------|
| `connected` | Healthy with client, capabilities, cleanup fn |
| `failed` | Connection failed with error |
| `needs-auth` | OAuth required |
| `pending` | Connection in progress |
| `disabled` | User explicitly disabled |

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/mcp/config.ts` | 7-scope discovery, policy filtering |
| `src/services/mcp/client.ts` | Connection management, tool fetching, call execution |
| `src/services/mcp/types.ts` | Zod schemas for transports and states |
| `src/services/mcp/auth.ts` | OAuth (PKCE, refresh, revocation) |
| `src/tools/MCPTool/MCPTool.ts` | Template tool definition |

## Cross-References

- [MCP Transports](transports.md) -- All 8 transport types
- [MCP Server Lifecycle](server_lifecycle.md) -- Connection management
- [MCP Channels](channels.md) -- Channel push system

## Interesting Findings

**Enterprise exclusive mode.** If `managed-mcp.json` exists, ALL other scopes are suppressed -- enterprise has total control over which MCP servers can be used.

**SDK servers can override builtins.** When `CLAUDE_AGENT_SDK_MCP_NO_PREFIX` is set, MCP tools skip the `mcp__` prefix, allowing them to replace built-in tools by name.

**MCP instructions break the prompt cache.** The MCP instructions section is the ONLY uncached dynamic section in the system prompt. Every turn it recomputes, and if the value changes, the entire prompt cache is invalidated.
