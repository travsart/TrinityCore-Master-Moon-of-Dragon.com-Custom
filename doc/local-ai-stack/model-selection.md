# Model Selection — Why These Models

> Decision log for local AI model choices. Updated April 5, 2026.

## Hardware Constraint

RTX 5090: 32GB GDDR7 VRAM, 1,792 GB/s bandwidth.
Only one large model loaded at a time. Must leave headroom for KV cache / context window.

## Primary: Qwen 3.5 27B (Q4_K_M) — Legal Reasoning

**Why Qwen 3.5 over Gemma 4 31B for reasoning:**

| Benchmark | Qwen 3.5 27B | Gemma 4 31B | Winner |
|-----------|-------------|-------------|--------|
| MMLU Pro | 86.1% | 85.2% | Qwen |
| GPQA Diamond | 85.5% | 84.3% | Qwen |
| AIME 2026 | — | 89.2% | Gemma (math) |
| Codeforces | — | 2150 ELO | Gemma (code) |
| Inference speed | ~35-45 tok/s | ~25-30 tok/s | Qwen |

Qwen wins on knowledge-heavy reasoning (MMLU-Pro, GPQA) which maps directly to
legal document analysis. Gemma wins on math/code which matters less for case work.

**Why Q4_K_M over Q8_0:**
- Q8_0 = 30GB → only ~2GB for context (terrible for RAG)
- Q4_K_M = 18GB → ~14GB for context (excellent for feeding retrieved chunks)
- For RAG, context headroom > quant precision

**Why not Q6_K:**
- No official `qwen3.5:27b-q6_K` on Ollama
- Only community upload (`frob/qwen3.5:27b-ud-q6_K_XL`) — not fully supported

## Bulk/Agent: Gemma 4 26B-A4B MoE — Background Processing

**Why MoE over Dense:**
- 25.2B total params, only 3.8B active per token
- Runs at ~4B-class speed with ~97% of 31B quality
- Perfect for bulk operations: indexing 1,579 files, auto-frontmatter, embeddings
- Lower power/heat for always-on OpenClaw agent

**Benchmarks (26B MoE vs 31B Dense):**

| Metric | 26B-A4B MoE | 31B Dense |
|--------|------------|-----------|
| MMLU Pro | 82.6% | 85.2% |
| Arena ELO | 1441 | 1452 |
| Active params/token | 3.8B | 30.7B |
| Inference speed | ~4B-class | Full 31B |

The 11-point ELO gap is invisible for document classification and indexing tasks.

## Embedding: nomic-embed-text — RAG Retrieval

**Why nomic-embed-text:**
- 1,024 dimensions, 8K token context
- Benchmarked on real legal data: matched OpenAI's paid API (only ~5% gap on legal text)
- 274MB — can stay resident alongside any large model
- Chunking strategy matters more than embedding model choice

**Alternatives considered:**
- `mxbai-embed-large`: Similar quality, slightly larger
- `Qwen3-Embedding-8B`: MTEB multilingual leader, but much larger (~8GB)
- `all-minilm`: Too small (384 dims) for complex legal docs

## Planned: DeepSeek-R1 32B — Chain-of-Thought Specialist

**Why add this:**
- Explicit chain-of-thought: shows reasoning step-by-step through retrieved chunks
- Outperformed Qwen and Llama on legal document analysis in benchmarks
- Best for "find what I missed" and cross-referencing tasks
- Use case: hard legal questions where you need visible reasoning

**Tradeoff**: ~20GB at Q4, slower inference. Use only for hard questions.

## Rejected: Fine-Tuning

**Why not fine-tune a "CaseLaw-Gemma":**
1. VRAM math: fine-tuning 31B needs 80-120GB (3-4x inference VRAM)
2. Data problem: 1,579 raw files need conversion to training pairs
3. Overfitting risk: model becomes great at one case, worse at general reasoning
4. Time cost: weeks of pipeline work vs. ADSCD deadline Aug 10, 2026
5. RAG solves the same problem without touching model weights

## Rejected: Llama 4 Scout

- 109B total / 17B active MoE — barely fits 32GB at Q6_K
- Slowest inference of all options despite only 17B active
- Restrictive license (700M MAU limit, branding requirements)
- Generally trails Qwen on reasoning benchmarks

## Multi-Model Strategy

Ollama swaps models in ~2-5 seconds. Run the right model for the right task:

```
Legal reasoning, analysis  → qwen3.5:27b-q4_K_M
Bulk processing, indexing  → gemma4:26b
Deep chain-of-thought      → deepseek-r1:32b (planned)
Embeddings (always loaded) → nomic-embed-text
```

## Sources

- Gemma 4 blog: https://blog.google/innovation-and-ai/technology/developers-tools/gemma-4/
- Gemma 4 benchmarks: https://avenchat.com/blog/gemma-4-31b-vs-26b-vs-e4b
- Qwen 3.5 vs Gemma 4: https://www.lushbinary.com/blog/gemma-4-vs-llama-4-vs-qwen-3-5-open-weight-model-comparison/
- Legal LLM comparison: https://www.siliconflow.com/articles/en/best-open-source-LLM-for-Legal-Document-Analysis
- Legal embedding benchmark: https://medium.com/@TheWake/openai-vs-ollama-i-benchmarked-both-embedding-models-on-real-legal-data-8eb01ccb272f
- MTEB Leaderboard: https://huggingface.co/spaces/mteb/leaderboard
