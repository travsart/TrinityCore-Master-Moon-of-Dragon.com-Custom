# Local AI Stack — VoxCore

> Ollama + Open WebUI + RAG + MCP + OpenClaw/NemoClaw
> Installed: April 5, 2026 | Hardware: RTX 5090 32GB VRAM

## Overview

Local AI inference stack for zero-cost bulk processing, private legal case work,
document RAG, and always-on background agents. Claude Code remains the orchestrator;
local models handle the grunt work via MCP bridge.

## Architecture

```
Open WebUI (localhost:3000) ─── Master UI, ChatGPT-like interface
    │
    ├── Ollama (localhost:11434) ─── Model server
    │     ├── qwen3.5:27b-q4_K_M ── Legal reasoning, RAG queries (~18GB VRAM)
    │     ├── gemma4:26b ─────────── Bulk processing, always-on agent (~15-18GB)
    │     ├── nomic-embed-text ───── Text embeddings, 768 dims (274MB)
    │     └── (planned) deepseek-r1:32b ── Chain-of-thought reasoning
    │
    ├── ChromaDB ─── Vector store (built into Open WebUI)
    │     ├── Hybrid search: BM25 (exact terms) + semantic (meaning)
    │     └── Collections: case_evidence, regulations, emails, medical_records
    │
    ├── Claude Code ─── Orchestrator (MCP bridge)
    │     ├── mcp-local-llm or OllamaClaude MCP server
    │     ├── Delegates bulk/cheap work to local models
    │     └── Keeps Anthropic API for hard reasoning only
    │
    └── OpenClaw + NemoClaw ─── Always-on agent (planned)
          ├── Background: continuously index new files
          ├── Background: find contradictions in evidence
          ├── Background: flag missing cross-references
          └── Privacy guardrails on all case data
```

## Documents in This Folder

| File | Contents |
|------|----------|
| `README.md` | This file — architecture overview |
| `ollama-setup.md` | Installation, config, model inventory |
| `model-selection.md` | Why we chose each model, benchmarks, alternatives |
| `rag-strategy.md` | Embedding types, chunking, vector DB, retrieval pipeline |
| `mcp-bridge.md` | Claude Code ↔ Ollama integration plan |
| `openclaw-plan.md` | OpenClaw/NemoClaw always-on agent architecture |
| `the-master-plan.txt` | Original architecture overview (user's notes from initial planning) |
| `capability-comparison.md` | Claude Opus 4.6 vs local 27B — parameters, tools, effective capability |
| `field-test-qwen-identity.md` | Field test results: Qwen's identity confusion when running locally |
| `qwen-conversation-transcript.txt` | Full transcript of first Qwen 3.5 local conversation |
| `handoff-prompt-for-local-models.md` | System prompt to onboard local models into the Triad |
