# Handoff — docs-rag MCP Server
**Generated**: 2026-04-11
**Branch**: master
**Last commit**: `0f0b574aee` chore(session-state): log CalmCore-245 tab (session 245)

## What Was Done (This Session)

Built a new **docs-rag MCP server** that exposes semantic vector search over `C:/Users/atayl/Desktop/IMPORTANT DOCS/` via ChromaDB + Ollama embeddings. This makes RAG search available as MCP tools (accessible to all subagents, skills, and tool calls) instead of only via the `/rag-search` slash command.

### Files Created
- `tools-dev/docs-rag/docs_rag_server.py` — FastMCP entry point, 6 `@mcp.tool()` wrappers + hot-reload
- `tools-dev/docs-rag/docs_rag_logic.py` — All implementation (~380 lines)

### Files Modified
- `.mcp.json` — Added `docs-rag` server entry
- `.claude/settings.local.json` — Added `docs-rag` to `enabledMcpjsonServers`

### 6 MCP Tools
| Tool | What |
|------|------|
| `docs_rag_search(query, top_k, doc_type, folder)` | Semantic vector search via Ollama + ChromaDB |
| `docs_rag_read(rel_path, source_root)` | Read full extracted text for a document |
| `docs_rag_list(folder, ext)` | Browse files with type/size metadata |
| `docs_rag_status()` | Dashboard: per-folder extraction + chunk counts |
| `docs_rag_rebuild(folder, extract_only)` | Background extraction + indexing |
| `docs_rag_reload()` | Hot-reload logic without MCP restart |

### Verified
- All 4 read-path tools tested and working (status, search, read, list)
- Graceful Ollama-down handling (friendly error message)
- Server starts clean (`FastMCP 3.0.2`, stdio transport)

## Current State
- **Build**: N/A (no C++ changes)
- **Server**: Not applicable (MCP server, auto-starts with Claude Code)
- **Uncommitted files**: 4 new/modified (the 2 server files + 2 config edits). Plus many `D` (deleted) `src/` files from the VoxCore/CalmCore domain split — those are pre-existing, not from this session.
- **Ollama**: Started during this session (PID 2048), may need restart

## Priority Work for This Tab

### 1. Commit the docs-rag server files
```
git add tools-dev/docs-rag/docs_rag_server.py tools-dev/docs-rag/docs_rag_logic.py .mcp.json .claude/settings.local.json
git commit -m "feat(mcp): docs-rag semantic search server for IMPORTANT DOCS"
```

### 2. Populate the remaining 5 folders (Ollama must be running)
The vector index only has Angel_VA (1,054 chunks) + legacy sweep data (8,028 chunks). Five folders need extraction:
```
# From Claude Code, just call:
docs_rag_rebuild()
# Or folder-by-folder:
docs_rag_rebuild("Case_Reference")  # largest — 654 extractable files
docs_rag_rebuild("Finances")
docs_rag_rebuild("Career")
docs_rag_rebuild("Brand")
```
Ethical_AI_Research and Resume Stuff are all `.md` — no extraction needed (already in arcanum).

### 3. Verify via MCP after restart
After restarting Claude Code (to load the new MCP server):
```
docs_rag_status()          # Should show the server is live
docs_rag_search("NPDB 30 day report")  # Should return hits
```

## Key Context
- **Architecture**: Follows arcanum's two-file pattern (server.py + logic.py with hot-reload). NOT the voxcore-db packaged module pattern.
- **ChromaDB**: File-backed `PersistentClient` at `.cache/rag/chroma/`, collection `important_docs`. No ChromaDB server needed.
- **Embedding**: Ollama `nomic-embed-text` (768-dim) at `localhost:11434`. First call after Ollama start takes ~4s (model loading), then ~100ms.
- **Concurrent access**: ChromaDB SQLite backend locks during writes. Search catches `sqlite3.OperationalError` and returns friendly message.

## Files You'll Need
- `tools-dev/docs-rag/docs_rag_logic.py` — All tool implementations (edit this, reload with `docs_rag_reload()`)
- `tools-dev/docs-rag/docs_rag_server.py` — FastMCP wrappers (edit only to add/change tool signatures)
- `.mcp.json` — MCP server registration
- `tools/rag_query.py` — Reference: the CLI query tool this wraps
- `tools/rag_build.py` — Reference: the index builder called by `docs_rag_rebuild()`
- `tools/extract_cache.py` — Reference: the extraction pipeline called by `docs_rag_rebuild()`

## Don't Touch (Other Tab Owns)
- `Case_Reference/` actual files — Case-SME owns, read-only
- `memory/case-*.md` — Case-SME owns (edits via patch files only)
- `tools/mbox/*`, `tools/unredact/*` — Mbox-Fast tab owns
- CalmCore `src/` anything — separate repo, separate tabs

## Current Index Status (from `docs_rag_status()`)
| Folder | Files | Extractable | Extracted | Chunks | Gap |
|--------|-------|-------------|-----------|--------|-----|
| Angel_VA | 65 | 48 | 48 | 1,054 | OK |
| Case_Reference | 1916 | 654 | 88 | 7,356 | 566 unextracted |
| Finances | 148 | 46 | 0 | 153 | 46 unextracted |
| Career | 67 | 52 | 0 | 66 | 52 unextracted |
| Brand | 54 | 26 | 0 | 45 | 26 unextracted |
| Ethical_AI_Research | 8 | 0 | 0 | 0 | all MD |
| Resume Stuff | 7 | 0 | 0 | 0 | all MD |
