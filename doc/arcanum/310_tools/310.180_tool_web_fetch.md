---
description: "WebFetchTool — URL fetching, HTML to markdown, Haiku processing, 15-minute cache, redirect handling, PDF binary content, deferred tool, prompt-based extraction"
title: "WebFetchTool -- Arcanum Wiki"
tags: [tools, url-fetching, html-to, haiku-processing, 15-minute-cache, redirect-handling, pdf-binary, deferred-tool]
---

# WebFetchTool -- Arcanum Wiki

## Purpose

WebFetchTool fetches content from a URL, converts HTML to markdown, and processes it through a smaller model (Haiku) with the user's prompt. It handles redirects, binary content (PDFs), and maintains a 15-minute cache. This tool is deferred (requires ToolSearch first).

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `url` | string | Yes | Fully-formed URL to fetch (HTTP auto-upgraded to HTTPS) |
| `prompt` | string | Yes | What to extract from the page content |

## Execution Flow

1. **Validation**: Parses URL to ensure it's valid.
2. **Permission check**: Checks preapproved hosts, then per-hostname allow/deny/ask rules.
3. **Fetch**: `getURLMarkdownContent()` fetches the URL with caching (15-minute self-cleaning cache).
4. **Redirect handling**: If the response is a cross-host redirect, returns a message instructing the model to re-fetch from the new URL (does not follow automatically).
5. **Content processing**: For preapproved URLs serving `text/markdown` under `MAX_MARKDOWN_LENGTH`, returns content raw. Otherwise, processes through `applyPromptToMarkdown()` which uses a small fast model.
6. **Binary content**: PDFs and other binary content are saved to disk with a mime-derived extension, and a note is appended to the result.

## Key Implementation Details

### Preapproved Hosts
The `isPreapprovedHost()` function (from `preapproved.ts`) maintains a list of trusted hosts that skip permission prompts. For preapproved URLs serving markdown, the content is returned without LLM processing, saving a model call.

### Permission Model
WebFetchTool has a hostname-based permission model. The `webFetchToolInputToPermissionRuleContent()` function extracts the hostname from the URL to create `domain:hostname` permission rule keys. Users can allow/deny entire domains.

### Redirect Detection
Cross-host redirects are NOT followed automatically. Instead, the tool returns structured information about the redirect (original URL, redirect URL, status code) and instructs the model to make a new WebFetch request. This preserves user visibility and permission control for the new domain.

### Content Processing
The `applyPromptToMarkdown()` function sends the fetched markdown content to a smaller/faster model (Haiku-class) with the user's prompt, producing a focused summary. This is important because raw web pages often contain navigation, ads, and boilerplate that would waste context tokens.

## Limits and Constraints

| Limit | Value |
|-------|-------|
| maxResultSizeChars | 100,000 |
| `shouldDefer` | true (requires ToolSearch) |
| Concurrency safe | Yes |
| Read only | Yes |
| Cache duration | 15 minutes (self-cleaning) |

## Permission Requirements

Per-hostname permission rules. Preapproved hosts bypass permission prompts. The permission UI offers suggestions to add `domain:hostname` allow rules to local settings.

## Error Handling

- **Invalid URL**: Validation rejects with "Invalid URL" message (errorCode 1)
- **Redirect**: Returns redirect info instead of error
- **Fetch failure**: HTTP error codes propagated in output (`code`, `codeText` fields)

## Interesting Findings

1. The tool prompt ALWAYS includes the auth warning about authenticated URLs, regardless of whether ToolSearch is active. A comment explains: conditionally toggling this prefix caused the tool description to flicker between SDK calls, invalidating the API prompt cache -- "two consecutive cache misses per flicker event."

2. The `shouldDefer: true` flag means WebFetchTool is not in the initial tool schema -- the model must use ToolSearch to discover it. This keeps the initial prompt smaller for sessions that don't need web fetching.

3. Binary content (PDFs, etc.) saved to disk enables the model to read them later with FileReadTool, creating a two-step pipeline: WebFetch saves the PDF, then Read renders it.
