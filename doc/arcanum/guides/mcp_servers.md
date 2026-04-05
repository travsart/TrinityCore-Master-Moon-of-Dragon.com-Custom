---
description: "MCP server guide — building connecting, stdio transport, tool registration, server instructions, OAuth PKCE, 27.8h timeout, NotebookLM MCP vision"
---

# Guide: MCP Servers — Building & Connecting — Arcanum Wiki

> How Claude Code discovers, connects, and communicates with MCP (Model Context Protocol) servers, and how to build your own.

## What Is MCP?

MCP (Model Context Protocol) is an open protocol that lets Claude Code connect to external services — databases, APIs, code intelligence engines, anything. MCP servers expose tools, resources, and prompts that Claude can use just like built-in tools.

## How Claude Code Discovers MCP Servers

MCP servers are configured in JSON files. Claude Code checks **7 configuration scopes** (in priority order):

| Scope | File | Who Sets It |
|-------|------|------------|
| 1. Managed | Enterprise policy | Admin |
| 2. User global | `~/.claude/settings.json` | User |
| 3. User local | `~/.claude/settings.local.json` | User |
| 4. Project | `.claude/settings.json` (in repo) | Team |
| 5. Project local | `.claude/settings.local.json` (in repo) | User |
| 6. MCP config | `.claude/mcp.json` (in repo) | Team |
| 7. Auto-discovered | Via plugins/DXT extensions | Automatic |

### Configuration Format

In any settings file:

```json
{
  "mcpServers": {
    "my-server": {
      "command": "python",
      "args": ["tools/mcp_servers/my_server.py"],
      "env": {
        "MY_API_KEY": "..."
      }
    }
  }
}
```

Or in `.claude/mcp.json` (dedicated MCP config):

```json
{
  "mcpServers": {
    "my-server": {
      "command": "node",
      "args": ["my-mcp-server.js"],
      "env": {}
    }
  }
}
```

## Transport Types (8 Supported)

| Transport | How It Connects | Use Case |
|-----------|----------------|----------|
| **stdio** | Spawns process, communicates via stdin/stdout | Local scripts (most common) |
| **sse** | Server-Sent Events over HTTP | Remote servers |
| **streamable** | Bidirectional streaming | Advanced remote servers |
| **websocket** | WebSocket connection | Real-time services |
| **http** | HTTP request/response | Simple REST wrappers |
| **docker** | Docker container | Isolated environments |
| **npx** | Runs via npx | npm-published servers |
| **uvx** | Runs via uvx (Python) | Python-published servers |

## Server Lifecycle

```
Claude Code starts
  → Reads all 7 config scopes
  → Merges MCP server definitions
  → For each server:
      → Spawn process (or connect to remote)
      → Send initialize request
      → Receive server capabilities (tools, resources, prompts)
      → Register server's tools in tool registry
      → Server instructions injected into system prompt
  → Tools available as mcp__{server}__{tool_name}
```

### Tool Naming Convention

MCP tools appear in Claude's tool list as:
```
mcp__{server-name}__{tool-name}
```

Example: A server named `voxcore-db` with a tool named `query` becomes `mcp__voxcore-db__query`.

### Server Instructions

MCP servers can provide instructions that get injected into the system prompt under `# MCP Server Instructions`. This is how servers tell Claude how to use their tools.

## Building an MCP Server (Python Example)

Here's a minimal MCP server in Python:

```python
#!/usr/bin/env python3
"""Minimal MCP server example."""
import json
import sys

def read_message():
    """Read a JSON-RPC message from stdin."""
    line = sys.stdin.readline()
    if not line:
        return None
    return json.loads(line)

def write_message(msg):
    """Write a JSON-RPC message to stdout."""
    sys.stdout.write(json.dumps(msg) + "\n")
    sys.stdout.flush()

def handle_initialize(msg):
    return {
        "jsonrpc": "2.0",
        "id": msg["id"],
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {}
            },
            "serverInfo": {
                "name": "my-server",
                "version": "1.0.0"
            }
        }
    }

def handle_tools_list(msg):
    return {
        "jsonrpc": "2.0",
        "id": msg["id"],
        "result": {
            "tools": [
                {
                    "name": "hello",
                    "description": "Say hello to someone",
                    "inputSchema": {
                        "type": "object",
                        "properties": {
                            "name": {
                                "type": "string",
                                "description": "Name to greet"
                            }
                        },
                        "required": ["name"]
                    }
                }
            ]
        }
    }

def handle_tool_call(msg):
    params = msg.get("params", {})
    tool_name = params.get("name")
    arguments = params.get("arguments", {})

    if tool_name == "hello":
        name = arguments.get("name", "world")
        return {
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": {
                "content": [
                    {
                        "type": "text",
                        "text": f"Hello, {name}! Nice to meet you."
                    }
                ]
            }
        }

def main():
    while True:
        msg = read_message()
        if msg is None:
            break

        method = msg.get("method")

        if method == "initialize":
            write_message(handle_initialize(msg))
        elif method == "tools/list":
            write_message(handle_tools_list(msg))
        elif method == "tools/call":
            write_message(handle_tool_call(msg))
        elif method == "initialized":
            pass  # Notification, no response needed

if __name__ == "__main__":
    main()
```

Register it in `.claude/settings.local.json`:

```json
{
  "mcpServers": {
    "my-server": {
      "command": "python",
      "args": ["path/to/server.py"]
    }
  }
}
```

## MCP Server Best Practices

### 1. Provide Server Instructions

Include an `instructions` field in your server info so Claude knows how to use your tools:

```python
"serverInfo": {
    "name": "voxcore-db",
    "version": "1.0.0",
    "instructions": "Database server for VoxCore. Use 'database' parameter (auth/characters/world/hotfixes/roleplay). Do NOT use schema.table notation."
}
```

### 2. Validate Inputs Thoroughly

Claude will sometimes pass malformed inputs. Always validate and return clear error messages:

```python
if not arguments.get("database"):
    return error_result(msg["id"], "Missing required 'database' parameter")
```

### 3. Keep Tool Results Reasonable

Claude Code has a 50K character per-tool result limit. If your query might return huge results, paginate or truncate:

```python
if len(result_text) > 40000:
    result_text = result_text[:40000] + "\n\n[TRUNCATED — use row_limit parameter]"
```

### 4. Use Descriptive Tool Schemas

The more context in your `inputSchema`, the better Claude uses your tool:

```python
{
    "name": "query",
    "description": "Execute SQL against a VoxCore database. Returns columns, rows, timing. Default 1000 row limit.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "database": {
                "type": "string",
                "description": "Target database: auth, characters, world, hotfixes, or roleplay",
                "enum": ["auth", "characters", "world", "hotfixes", "roleplay"]
            },
            "sql": {
                "type": "string",
                "description": "SQL query to execute"
            },
            "row_limit": {
                "type": "integer",
                "description": "Max rows to return (default 1000, max 50000)",
                "default": 1000
            }
        },
        "required": ["database", "sql"]
    }
}
```

## Key Internals

### Timeout

MCP tool calls have a default timeout of approximately **27.8 hours** (`MAX_TOOL_RESULT_TOKENS * some factor`). Effectively infinite for normal use. Override with `MCP_TOOL_TIMEOUT` env var.

### OAuth Support

MCP servers can require OAuth authentication. Claude Code supports PKCE flow for MCP OAuth:

```json
{
  "mcpServers": {
    "authenticated-server": {
      "url": "https://api.example.com/mcp",
      "transport": "sse",
      "oauth": {
        "authorizationUrl": "https://example.com/oauth/authorize",
        "tokenUrl": "https://example.com/oauth/token",
        "clientId": "my-client-id",
        "scopes": ["read", "write"]
      }
    }
  }
}
```

### Channel Push (6-Layer Gating)

MCP servers can push notifications through a channel system with 6 layers of gating. This enables real-time updates from servers (e.g., file watchers, build status).

### The NotebookLM MCP Vision

The ultimate goal for Arcanum: build an MCP server that serves the wiki as searchable knowledge. Every Claude Code tab gets instant recall of all 200+ articles without loading them into context.

```
Claude Code tab → MCP tool call: arcanum_search("compaction tiers")
  → MCP server reads doc/arcanum/core/compaction_tiers.md
  → Returns relevant content
  → Claude has the knowledge without spending context tokens
```

This is the endgame.

## Cross-References

- [MCP Client Architecture](../mcp/overview.md) — full technical internals
- [MCP Transports](../mcp/transports.md) — all 8 transport types
- [MCP OAuth](../mcp/oauth.md) — authentication flow
- [MCP Channels](../mcp/channels.md) — push notification system
