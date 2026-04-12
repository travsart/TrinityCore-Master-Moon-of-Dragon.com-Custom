# MCP Bridge — Claude Code ↔ Ollama

> Plan for connecting Claude Code to local Ollama models via MCP.

## Concept

Claude Code stays the brain/orchestrator. Local models handle bulk work.
Up to 98.75% reduction in Anthropic API token usage for mechanical tasks.

```
Claude Code (Opus 4.6, 1M context)
    │
    ├── MCP: Ollama → delegate bulk inference, embeddings, classification
    ├── MCP: VoxCore DB → MySQL queries (already working)
    ├── MCP: Arcanum → knowledge base (already working)
    └── MCP: Codeintel → C++ symbols (already working)
```

## MCP Server Options

### Option 1: mcp-local-llm (recommended)
- **Repo**: https://github.com/aplaceforallmystuff/mcp-local-llm
- Claude sits at the top as orchestrator
- Forwards requests to Ollama (or any OpenAI-compatible backend)
- Claude decides what to delegate and reviews what comes back
- File-aware tools for 98.75% token reduction

### Option 2: OllamaClaude
- **Repo**: https://github.com/Jadael/OllamaClaude
- Specifically built for Claude Code + Ollama integration
- Exposes tools: `ollama_generate`, `ollama_chat`, `ollama_embeddings`
- Claude delegates coding/bulk tasks, reviews/refines results

### Option 3: ollama-mcp (raw SDK)
- **Repo**: https://github.com/rawveg/ollama-mcp
- Exposes complete Ollama SDK as MCP tools
- More low-level — good if we want full control

## Delegation Rules (add to CLAUDE.md)

```
When a task is mechanical/bulk and doesn't require frontier reasoning:
- Document classification → delegate to local gemma4:26b
- Embedding generation → delegate to local nomic-embed-text
- Text extraction/summarization → delegate to local qwen3.5:27b-q4_K_M
- Frontmatter generation → delegate to local gemma4:26b
- Simple Q&A over retrieved chunks → delegate to local model

When a task requires frontier reasoning:
- Legal strategy analysis → keep on Claude API
- Complex cross-referencing → keep on Claude API
- Multi-step reasoning chains → keep on Claude API
- Code generation → keep on Claude API
```

## Configuration

Add to `.claude/settings.json`:
```json
{
  "mcpServers": {
    "ollama": {
      "command": "npx",
      "args": ["-y", "mcp-local-llm"],
      "env": {
        "OLLAMA_BASE_URL": "http://localhost:11434"
      }
    }
  }
}
```

## Next Steps

1. Choose MCP server (likely mcp-local-llm)
2. Install and configure
3. Test delegation with simple tasks
4. Add delegation rules to CLAUDE.md
5. Measure API token savings over a session
