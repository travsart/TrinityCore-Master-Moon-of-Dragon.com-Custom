# RAG Strategy — Legal Case Files

> Retrieval-Augmented Generation pipeline for 1,579 case files.

## Why RAG (Not Fine-Tuning)

- Zero training needed — models stay general-purpose and smart
- All case knowledge accessible via semantic search
- Instant updates — add a document, re-embed it, done
- 100% local — nothing leaves the machine
- Works with current hardware (single RTX 5090)

## Three Embedding Layers

### Layer 1: Text Embeddings (covers ~70% of files)
- **Model**: `nomic-embed-text` via Ollama (768 dims, 8K context)
- **Handles**: Plain text, extracted PDF text, emails, markdown, DOCX
- **Chunking**: 500 tokens for dense legal PDFs, 1500 for general docs
- **Built into Open WebUI** — drag and drop files, auto-embedded

### Layer 2: Vision Embeddings (planned — scanned documents)
- **Model**: ColQwen2 or ColPali
- **Handles**: Scanned forms, DD-214s, MFRs, medical records, photos of documents
- **How**: Treats entire document pages as images — no OCR, no text extraction
- **Why**: OCR butchers military forms with tables, signatures, stamps, handwritten notes
- **Storage**: Multi-vector (up to 768 vectors of 128 dims per page)

### Layer 3: Audio Embeddings (planned — recordings)
- **Pipeline**: Whisper (transcribe audio → text) → nomic-embed-text (embed text)
- **Handles**: Any audio recordings, voicemails, phone calls
- **No native audio-semantic search yet** — WavRAG/SEAL are research-only

## Vector Database: ChromaDB

**Why ChromaDB over LanceDB:**
- **BM25 + SPLADE hybrid search** — critical for legal work
  - Semantic search: "documents about whistleblower retaliation"
  - Exact term match: "10 USC 1034" or "DoDI 6025.13"
  - Both at once = hybrid search. ChromaDB does this natively
- LangChain integration for orchestration
- Rust rewrite (2025): 4x faster writes and queries
- Built into Open WebUI — zero setup
- 1,579 files is well within ChromaDB's sweet spot (struggles at 50M+ vectors)

**Collections plan:**
| Collection | Contents | Est. Chunks |
|-----------|----------|-------------|
| `case_evidence` | MFRs, investigations, binder docs | ~5,000 |
| `regulations` | DHA-PM, DoDI, USC, CFR extracts | ~2,000 |
| `emails` | Email correspondence, .eml attachments | ~3,000 |
| `medical_records` | Medical documentation | ~1,500 |
| `correspondence` | Letters, filings, complaints | ~1,000 |
| `timeline` | Dated events, chronologies | ~500 |

## Retrieval Pipeline

```
User Query
    │
    ├─ BM25 search (exact term matching)
    │     Returns: documents mentioning exact citations/names
    │
    ├─ Semantic search (vector similarity)
    │     Returns: documents conceptually related to query
    │
    ├─ Hybrid merge + reranking
    │     Combines both result sets, deduplicates, reranks
    │
    └─ Top-K chunks → LLM context
          Model generates answer grounded in retrieved evidence
```

## Two-Tier Routing

| Query Type | Route | Cost |
|-----------|-------|------|
| Lookup, citation, date finding, classification | Local (Gemma 4 / Qwen) | Free |
| Legal reasoning, strategy, gap analysis | Claude API | ~$0.01-0.05 |
| Contradiction finding, cross-referencing | Local (DeepSeek-R1) | Free |

~90% of queries are bulk/routine → local. ~10% need frontier intelligence → API.

## Chunking Guidelines

| Document Type | Chunk Size | Overlap | Notes |
|--------------|-----------|---------|-------|
| Dense legal PDFs | 500 tokens | 50 tokens | Preserve paragraph boundaries |
| Emails | 1000 tokens | 100 tokens | One email per chunk if possible |
| Regulations | 500 tokens | 100 tokens | Section-aware splitting |
| General documents | 1500 tokens | 150 tokens | Default |

## Key Insight

> The choice of vector database matters less than people think —
> chunking strategy and retrieval pipeline matter far more.
> — Multiple 2026 RAG benchmark studies

## Sources

- Open WebUI RAG docs: https://github.com/open-webui/open-webui
- ChromaDB vs LanceDB: https://4xxi.com/articles/vector-database-comparison/
- ColQwen2 PDF RAG: https://modal.com/docs/examples/chat_with_pdf_vision
- Legal embedding benchmark: https://medium.com/@TheWake/openai-vs-ollama-i-benchmarked-both-embedding-models-on-real-legal-data-8eb01ccb272f
- Multimodal RAG survey: https://github.com/llm-lab-org/Multimodal-RAG-Survey
