# Capability Comparison: Claude Opus 4.6 vs Local 27B Models

> April 5, 2026 — Honest assessment for architecture decision-making.

## Raw Model Comparison

| Dimension | Qwen 3.5 27B (Q4_K_M) | Gemma 4 26B MoE | Claude Opus 4.6 |
|-----------|----------------------|-----------------|-----------------|
| Parameters | 27B (all active) | 25.2B total / 3.8B active | Est. 500B-2T (MoE, subset active) |
| Context window | 32K practical | 32K practical | **1,000,000 tokens** |
| Training data | Good, open-weight | Good, Apache 2.0 | Massive, proprietary, RLHF |
| Self-correction | Poor (see field test) | Poor | Strong |
| Legal reasoning | Good | Moderate | Significantly better |
| Speed | 59 tok/s (RTX 5090) | 179 tok/s (RTX 5090) | ~30-50 tok/s (API) |
| Cost per token | $0.00 | $0.00 | ~$0.01-0.05/query |
| Privacy | 100% local | 100% local | Cloud (Anthropic servers) |
| MMLU Pro | 86.1% | 82.6% | Higher (not directly comparable) |

## Effective Capability (Not Just Parameters)

Raw parameter count is misleading. The real comparison is effective capability
for our specific use case (legal case work, system building, orchestration).

### Claude Opus 4.6 with Full Tool Access

Running in Claude Code with 1M context + integrated tools:

**Tools available in a single session:**
- Full filesystem read/write on local machine
- 5 MCP servers (MySQL DBs, code intelligence, Ollama, Arcanum wiki, Wago DB2)
- Web search and web fetch
- Subagent spawning (parallel workers)
- Bash/PowerShell execution
- LSP code navigation
- Git operations
- 30+ custom skills

**What this means:**
- Can hold an entire conversation + all research + all architecture decisions
  in a single 1M token context
- Can read, write, and execute code on the local machine
- Can query databases, search the web, spawn parallel agents
- Can self-correct when wrong (demonstrated in identity test)

**Effective capability estimate: ~2,000B-5,000B equivalent**
Not because the model is literally 5T parameters, but because:
- Larger base model (~20-50x more parameters)
- 30x larger context window
- 20+ integrated tools
- Agent spawning capability
- Strong self-correction / metacognition

### Qwen 3.5 27B Local (via Ollama)

Running standalone with no tools:
- Text prompt in → text response out
- No file access, no database queries, no web search, no agents
- 32K practical context window
- No self-awareness about deployment (proven in field test)

**Effective capability: 27B (what it says on the tin)**

### Gemma 4 26B MoE Local (via Ollama)

- Same constraints as Qwen (no tools)
- Only 3.8B params active per token (MoE efficiency)
- Screaming fast at 179 tok/s for classification tasks
- Best suited for bulk grunt work

**Effective capability for classification: punches above 27B due to MoE**
**Effective capability for reasoning: below Qwen 3.5**

## The Economic Argument

| Scenario | Best Tool | Why |
|----------|----------|-----|
| Process 1,579 case files overnight | **Local Gemma 4** | Free, 179 tok/s, no API cost |
| Classify 500 documents by type | **Local Gemma 4** | 3 seconds each = 25 min total, $0 |
| Generate frontmatter for all files | **Local Qwen** | Better writing quality, still free |
| "Find contradictions in evidence" | **Local Qwen** | Pattern matching, free at scale |
| "What's the legal strategy for DHA appeal?" | **Claude Opus** | Needs frontier reasoning |
| "Build me a 6-layer AI architecture" | **Claude Opus** | Needs tools, context, orchestration |
| "Is this evidence sufficient for AFBCMR?" | **Claude Opus** | Needs legal nuance + case context |
| Simple Q&A over retrieved chunks | **Local Qwen** | Good enough, free |

## The Two-Tier Architecture (Validated)

```
90% of work (grunt/bulk) → Local models ($0)
 - Classification, embedding, frontmatter, simple extraction
 - Qwen for quality, Gemma for speed

10% of work (hard reasoning) → Claude API (~$0.01-0.05/query)
 - Legal strategy, cross-referencing, system design
 - Needs frontier intelligence + tool access
```

**Cost savings estimate:**
- Before: 100% API tokens for everything
- After: 90% free locally, 10% API
- Potential monthly savings: significant reduction in the ~$680-800/mo AI budget

## Key Insight

> You don't use a cruise missile to hammer a nail.
> The 27B does the 90% that's muscle work.
> Claude handles the 10% that needs the full stack.

## The Flip Side — Why Local Models Still Matter

Despite being ~20-50x smaller, local models offer:
- **$0.00 per token** — unlimited use
- **59-179 tok/s on local GPU** — fast for batch work
- **100% privacy** — case data never leaves the machine
- **Always available** — no API outages, no rate limits
- **Overnight processing** — can grind through files while you sleep

These aren't weaknesses of local models — they're strengths that
frontier models literally cannot match. The architecture uses both.
