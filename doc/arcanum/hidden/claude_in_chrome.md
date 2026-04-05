---
description: "Claude in Chrome — browser extension integration, Chrome DevTools protocol, web page interaction from Claude Code CLI"
---

# Claude in Chrome -- Arcanum Wiki

## What Is This?

Claude in Chrome is a browser automation system that connects Claude Code to a Chrome extension, enabling Claude to control web pages in the user's actual browser. Unlike Computer Use (which operates at the pixel level), Claude in Chrome works at the DOM level -- it can navigate, click elements, fill forms, execute JavaScript, read page content, take screenshots, create GIFs, and manage tabs across 7 Chromium-based browsers. The connection works through a native messaging host bridge that creates a Unix socket (or Windows named pipe) between the Claude Code process and the Chrome extension.

## How It Works

### Architecture

The system has four layers:

1. **Chrome Extension** (`@ant/claude-for-chrome-mcp`) -- runs in the browser, executes commands
2. **Native Messaging Host** (`chromeNativeHost.ts`) -- bridges stdin/stdout with the extension via Chrome's native messaging protocol
3. **MCP Server** (`mcpServer.ts`) -- provides the tool interface that Claude Code's model calls
4. **Setup/Detection** (`setup.ts`, `common.ts`) -- auto-configuration and browser detection

### Browser Support (common.ts)

Seven Chromium-based browsers are supported with platform-specific paths for data directories, native messaging host manifests, and registry keys:

| Browser | macOS | Linux | Windows |
|---------|-------|-------|---------|
| Chrome | Yes | Yes | Yes |
| Brave | Yes | Yes | Yes |
| Arc | Yes | No | Yes |
| Edge | Yes | Yes | Yes |
| Chromium | Yes | Yes | Yes |
| Vivaldi | Yes | Yes | Yes |
| Opera | Yes (Roaming) | Yes | Yes (Roaming) |

Detection order: Chrome, Brave, Arc, Edge, Chromium, Vivaldi, Opera.

### Native Messaging Host (chromeNativeHost.ts)

The native host implements Chrome's native messaging protocol: 4-byte little-endian length prefix followed by JSON message. It creates a socket server that MCP clients connect to:

```typescript
const VERSION = '1.0.0'
const MAX_MESSAGE_SIZE = 1024 * 1024 // 1MB

class ChromeNativeHost {
  private mcpClients = new Map<number, McpClient>()
  private server: Server | null = null
}
```

Message flow:
- Chrome extension sends tool responses/notifications via stdin
- MCP clients (Claude Code sessions) connect via Unix socket or named pipe
- Tool requests from MCP clients are forwarded to Chrome via stdout
- Tool responses from Chrome are forwarded back to all connected MCP clients

Socket paths:
- Unix: `/tmp/claude-mcp-browser-bridge-{username}/{pid}.sock` (per-process)
- Windows: `\\.\pipe\claude-mcp-browser-bridge-{username}`

Stale socket cleanup: On startup, scans the socket directory for `.sock` files, checks if the PID is alive via `process.kill(pid, 0)`, and removes dead sockets.

### MCP Server (mcpServer.ts)

The MCP server provides the tool interface via `@ant/claude-for-chrome-mcp`:

```typescript
export function createChromeContext(env?: Record<string, string>): ClaudeForChromeContext {
  return {
    serverName: 'Claude in Chrome',
    socketPath: getSecureSocketPath(),
    getSocketPaths: getAllSocketPaths,
    clientTypeId: 'claude-code',
    // ...
  }
}
```

**Bridge mode**: For Anthropic employees or when `tengu_copper_bridge` GrowthBook flag is enabled, the extension connects via WebSocket to `wss://bridge.claudeusercontent.com` instead of native messaging. This enables remote browser control.

**Lightning mode** (ant-only): The MCP server can run a "lightning-mode agent loop" in Node using `sideQuery()` for inference, calling the extension's `lightning_turn` tool for execution. This is triple-gated: build-time in the extension, runtime in the MCP server, and tool-listing filter.

**Permission modes**: `ask`, `skip_all_permission_checks`, `follow_a_plan`

### Setup and Auto-Enable (setup.ts)

Auto-enable conditions:
1. Interactive session
2. Chrome extension detected as installed (cached check)
3. Anthropic employee OR `tengu_chrome_auto_enable` GrowthBook flag

Extension installation detection scans all browser profiles for the extension ID directories:
- Prod: `fcoeoabgfenejglbffodgkkbkcdhcgfn`
- Dev: `dihbgbndebgnbjfmelmegjepbnkhlgni`
- Ant: `dngcpimnedloihjnnfngkgjoidhnaolf`

The native host manifest is installed to all detected browsers' NativeMessagingHosts directories. On Windows, registry entries are also created.

### Available Tools (toolRendering.tsx)

17 browser tools: `javascript_tool`, `read_page`, `find`, `form_input`, `computer`, `navigate`, `resize_window`, `gif_creator`, `upload_image`, `get_page_text`, `tabs_context_mcp`, `tabs_create_mcp`, `update_plan`, `read_console_messages`, `read_network_requests`, `shortcuts_list`, `shortcuts_execute`.

### System Prompt (prompt.ts)

The prompt includes detailed instructions for browser automation, including:
- Always call `tabs_context_mcp` first to get current tab state
- Avoid triggering JavaScript alerts/confirms (they block the extension)
- Use console.log + `read_console_messages` for debugging instead
- Stop and ask the user after 2-3 failed attempts

When the `WebBrowser` tool is also available, a differentiation prompt is injected: "Use WebBrowser for dev servers, use claude-in-chrome for the user's real Chrome (logged-in sessions, OAuth, computer-use)."

## Feature Gating

| Gate | Type | Notes |
|------|------|-------|
| `--chrome` CLI flag | CLI argument | Explicit enable |
| `CLAUDE_CODE_ENABLE_CFC` | Env var | Enable/disable |
| `config.claudeInChromeDefaultEnabled` | Global config | Persistent preference |
| `tengu_chrome_auto_enable` | GrowthBook | Auto-enable for non-ants |
| `tengu_copper_bridge` | GrowthBook | WebSocket bridge mode |
| Extension installed | Detection | Required for auto-enable |

## User-Facing Behavior

1. Install the Claude in Chrome extension from `https://claude.ai/chrome`
2. Claude Code auto-detects the extension and enables browser tools
3. Claude can navigate pages, fill forms, click elements, execute JavaScript
4. Tab IDs are tracked; clickable "[View Tab]" links focus the relevant tab
5. GIF recording available for multi-step interactions
6. Console and network request monitoring

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/claudeInChrome/common.ts` | Browser detection, socket paths, 7-browser config |
| `src/utils/claudeInChrome/setup.ts` | Auto-enable logic, native host manifest installation |
| `src/utils/claudeInChrome/chromeNativeHost.ts` | Native messaging host implementation |
| `src/utils/claudeInChrome/mcpServer.ts` | MCP server with bridge, lightning, analytics |
| `src/utils/claudeInChrome/prompt.ts` | System prompt for browser automation |
| `src/utils/claudeInChrome/setupPortable.ts` | Portable extension detection (also used by VS Code) |
| `src/utils/claudeInChrome/toolRendering.tsx` | Custom rendering for 17 browser tools |

## Configuration

- `config.claudeInChromeDefaultEnabled` -- persistent enable/disable
- `config.cachedChromeExtensionInstalled` -- positive-only detection cache
- `config.chromeExtension.pairedDeviceId` -- bridge pairing
- `CLAUDE_CHROME_PERMISSION_MODE` env var -- permission mode override

## Interesting Findings

1. **Positive-only caching** for extension detection: only `true` results are cached to `~/.claude.json`. A negative result might come from a machine that shares the config but has no local Chrome (e.g., a remote dev environment using the bridge), and caching it would permanently poison auto-enable.

2. **The first-time install flow** detects manifest updates and opens `https://clau.de/chrome/reconnect` in the browser to restart the native host connection.

3. **Opera uses Roaming AppData** instead of Local on Windows -- a browser-specific quirk handled in the config.

4. **The lightning mode** for browser automation runs an entire agent loop with sideQuery (Anthropic API) plus the extension's execution primitives. This is triple-gated with build-time, runtime, and tool-listing filters. It is essentially a browser agent running inside the MCP server.

5. **Tab tracking** across the session uses a bounded Set (max 200 tab IDs) that clears entirely when full, rather than using LRU eviction. Simple but effective.
