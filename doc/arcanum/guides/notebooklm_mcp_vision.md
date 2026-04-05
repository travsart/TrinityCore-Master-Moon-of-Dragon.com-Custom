---
description: "NotebookLM MCP vision — arcanum search server, TF-IDF grep, tool registration, context budget savings, implementation roadmap, knowledge persistence"
---

# Guide: The NotebookLM MCP Vision — Arcanum Wiki

> The plan to turn Arcanum into a persistent knowledge MCP server so every Claude Code tab has instant recall.

## The Problem

Right now, each Claude Code tab starts fresh. It loads MEMORY.md + up to 5 topic files, but that's ~6 files out of potentially hundreds in the Arcanum wiki. The wiki knowledge exists on disk but isn't accessible unless Claude happens to read the right file.

## The Solution: Arcanum MCP Server

Build an MCP server that:
1. Indexes all Arcanum wiki articles
2. Provides a search tool that finds relevant articles by query
3. Returns focused excerpts instead of entire files
4. Runs automatically whenever Claude Code starts

```
┌─────────────────────────────────────────────┐
│ Claude Code Tab                              │
│                                              │
│ "How does compaction work?"                  │
│   → mcp__arcanum__search("compaction")       │
│   → Returns: compaction_overview.md excerpt   │
│   → Claude now knows compaction details       │
│   → Context cost: ~500 tokens (not 15,000)   │
└─────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────┐
│ Arcanum MCP Server (Python)                  │
│                                              │
│ Tools:                                       │
│   arcanum_search(query, limit=3)             │
│   arcanum_read(article_path)                 │
│   arcanum_list(directory)                    │
│   arcanum_index()                            │
│                                              │
│ Index: TF-IDF or embedding-based search      │
│ Source: doc/arcanum/**/*.md                   │
└─────────────────────────────────────────────┘
```

## Architecture Design

### Option A: Simple TF-IDF Search (Recommended for v1)

```python
# arcanum_mcp_server.py
import json
import sys
from pathlib import Path
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.metrics.pairwise import cosine_similarity

ARCANUM_DIR = Path("C:/Users/atayl/VoxCore/doc/arcanum")

class ArcanumIndex:
    def __init__(self):
        self.documents = {}  # path → content
        self.vectorizer = TfidfVectorizer(stop_words='english')
        self.matrix = None
        self.paths = []

    def build(self):
        for md_file in ARCANUM_DIR.rglob("*.md"):
            content = md_file.read_text(encoding='utf-8')
            rel_path = str(md_file.relative_to(ARCANUM_DIR))
            self.documents[rel_path] = content
            self.paths.append(rel_path)

        corpus = [self.documents[p] for p in self.paths]
        self.matrix = self.vectorizer.fit_transform(corpus)

    def search(self, query, limit=3):
        query_vec = self.vectorizer.transform([query])
        scores = cosine_similarity(query_vec, self.matrix).flatten()
        top_indices = scores.argsort()[-limit:][::-1]
        results = []
        for idx in top_indices:
            if scores[idx] > 0.05:  # relevance threshold
                path = self.paths[idx]
                # Return first 2000 chars as excerpt
                excerpt = self.documents[path][:2000]
                results.append({
                    "path": path,
                    "score": float(scores[idx]),
                    "excerpt": excerpt
                })
        return results
```

### Option B: Embedding-Based Search (v2)

Use sentence-transformers for semantic search:

```python
from sentence_transformers import SentenceTransformer

model = SentenceTransformer('all-MiniLM-L6-v2')
embeddings = model.encode(documents)
# Cosine similarity against query embedding
```

This is more accurate but requires a larger dependency (torch).

### Option C: NotebookLM API Integration (The Dream)

If Google exposes a NotebookLM API:

```python
# Upload all Arcanum articles to a NotebookLM notebook
# Query via API: "How does the permission system work?"
# Get AI-synthesized answer grounded in the articles
```

This would give Claude Code access to NotebookLM's audio overview, suggested questions, and synthesis capabilities.

## MCP Server Implementation

### Tools to Expose

```python
TOOLS = [
    {
        "name": "arcanum_search",
        "description": "Search the Arcanum wiki for Claude Code internals knowledge. Returns relevant article excerpts.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Search query (e.g., 'compaction tiers', 'permission system', 'hook events')"
                },
                "limit": {
                    "type": "integer",
                    "description": "Max results (default 3)",
                    "default": 3
                }
            },
            "required": ["query"]
        }
    },
    {
        "name": "arcanum_read",
        "description": "Read a specific Arcanum wiki article by path.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Article path relative to arcanum/ (e.g., 'core/compaction_overview.md')"
                }
            },
            "required": ["path"]
        }
    },
    {
        "name": "arcanum_list",
        "description": "List all articles in an Arcanum wiki directory.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "directory": {
                    "type": "string",
                    "description": "Directory to list (e.g., 'core', 'tools', 'guides'). Omit for root."
                }
            }
        }
    }
]
```

### Server Instructions

```python
SERVER_INFO = {
    "name": "arcanum",
    "version": "1.0.0",
    "instructions": (
        "Arcanum is a comprehensive wiki of Claude Code's internal architecture, "
        "reverse-engineered from leaked source code. Use arcanum_search to find "
        "knowledge about Claude Code internals (compaction, permissions, hooks, "
        "tools, memory, agents, etc.). Use arcanum_read for full articles."
    )
}
```

### Registration

Add to `.claude/settings.local.json`:

```json
{
  "mcpServers": {
    "arcanum": {
      "command": "python",
      "args": ["tools/mcp_servers/arcanum_server.py"]
    }
  }
}
```

## Usage Flow

1. **Claude Code starts** → MCP server boots, indexes all Arcanum articles
2. **User asks about internals** → Claude calls `arcanum_search("how does memory selection work")`
3. **Server returns** → Top 3 matching articles with excerpts
4. **Claude synthesizes** → Uses the excerpts to give an informed answer
5. **Deep dive needed** → Claude calls `arcanum_read("core/memory_selector.md")` for full article

## Context Budget Impact

| Approach | Context Cost |
|----------|-------------|
| Load ALL articles into memory | ~500K tokens (impossible) |
| Memory selector picks 5 files | ~25K tokens |
| Arcanum MCP search (3 results) | ~3K tokens (just excerpts) |
| Arcanum MCP read (1 article) | ~2K tokens |

The MCP approach is **10x more context-efficient** than memory files.

## Implementation Roadmap

### Phase 1: Local TF-IDF (This Week)
- Build simple Python MCP server
- TF-IDF search over all Arcanum .md files
- Register in settings.local.json
- Test with real queries

### Phase 2: Better Search (Next Week)
- Add heading-level indexing (search within articles)
- Add cross-reference following
- Add "related articles" in results
- Optionally switch to embedding-based search

### Phase 3: NotebookLM Integration (When Available)
- Upload Arcanum articles to NotebookLM
- If API available: query via MCP tool
- If not: manual notebook for human learning

### Phase 4: Live Source Tracking
- Watch source archives for changes
- Auto-update articles when source changes
- Version tracking (v0.2.57 vs current release)

## Why This Matters

The Claude Code source was leaked from npm sourcemaps. Anthropic has likely patched this. The source archives on your Desktop are a finite, non-renewable resource. Arcanum is the knowledge extracted FROM those sources — and the MCP server makes that knowledge permanently accessible to every Claude Code tab, forever.

## Cross-References

- [MCP Server Guide](mcp_servers.md) — how to build MCP servers
- [Memory System](memory_mastery.md) — current memory limitations
- [Architecture Overview](../core/architecture.md) — what Arcanum documents
