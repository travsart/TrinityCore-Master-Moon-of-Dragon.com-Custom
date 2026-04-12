#!/usr/bin/env python3
"""Build a local RAG vector index from extracted/transcribed/OCR'd content.

Chunks documents, embeds them via Ollama (nomic-embed-text, 768 dims), and
stores in ChromaDB (persistent, file-backed, no server required).

Design:
  - Uses Ollama's /api/embed endpoint at http://localhost:11434
  - ChromaDB persistent client at .cache/rag/chroma/
  - One collection per sweep source (e.g. 'important_docs')
  - Chunk strategy: ~600 token sliding window with 100 token overlap
  - Metadata per chunk: source path, doc_type, chunk_idx, char_start, char_end

Sources (auto-discovered):
  .cache/extracted/*/files/              # PDF/DOCX/EML text
  .cache/transcribed/*/files/*.txt       # audio transcripts
  .cache/ocr/*/files/                    # image OCR
  AI_Studio/Reports/sme_importantdocs/extracted/*/   # legacy sweep extract

Usage:
  python tools/rag_build.py                        # build all default sources
  python tools/rag_build.py --collection case      # named collection
  python tools/rag_build.py --source <dir>         # custom source dir
  python tools/rag_build.py --stats                # show index stats, no build
  python tools/rag_build.py --drop                 # delete collection first

Requirements:
  pip install chromadb
  Ollama running at localhost:11434 with nomic-embed-text pulled
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHROMA_DIR = REPO_ROOT / ".cache" / "rag" / "chroma"
OLLAMA_URL = "http://localhost:11434"
EMBED_MODEL = "nomic-embed-text"
EMBED_DIM = 768
CHUNK_SIZE = 600  # approx tokens (we use chars/4 as proxy)
CHUNK_OVERLAP = 100
CHARS_PER_TOKEN = 4  # rough estimate for English


def chunk_text(text: str, chunk_size_tokens: int = CHUNK_SIZE, overlap_tokens: int = CHUNK_OVERLAP) -> list[dict]:
    """Split text into overlapping chunks. Returns list of {text, char_start, char_end, idx}."""
    chunk_chars = chunk_size_tokens * CHARS_PER_TOKEN
    overlap_chars = overlap_tokens * CHARS_PER_TOKEN
    chunks = []
    if not text or not text.strip():
        return chunks
    text_len = len(text)
    start = 0
    idx = 0
    while start < text_len:
        end = min(start + chunk_chars, text_len)
        # Try to break on a natural boundary (paragraph, sentence) near end
        if end < text_len:
            for delim in ("\n\n", ". ", "\n", " "):
                split = text.rfind(delim, start + chunk_chars // 2, end + 50)
                if split != -1:
                    end = split + len(delim)
                    break
        chunk = text[start:end].strip()
        if chunk:
            chunks.append({
                "text": chunk,
                "char_start": start,
                "char_end": end,
                "idx": idx,
            })
            idx += 1
        if end >= text_len:
            break
        start = max(start + 1, end - overlap_chars)
    return chunks


def discover_sources(include_legacy: bool = True) -> list[tuple[str, Path]]:
    """Find all extraction/transcription/OCR source dirs. Returns [(doc_type, root_dir), ...]."""
    sources = []
    cache_root = REPO_ROOT / ".cache"

    extracted = cache_root / "extracted"
    if extracted.exists():
        for sub in extracted.iterdir():
            files = sub / "files"
            if files.exists():
                sources.append(("extracted", files))

    transcribed = cache_root / "transcribed"
    if transcribed.exists():
        for sub in transcribed.iterdir():
            files = sub / "files"
            if files.exists():
                sources.append(("transcribed", files))

    ocr = cache_root / "ocr"
    if ocr.exists():
        for sub in ocr.iterdir():
            files = sub / "files"
            if files.exists():
                sources.append(("ocr", files))

    if include_legacy:
        legacy = REPO_ROOT / "AI_Studio" / "Reports" / "sme_importantdocs" / "extracted"
        if legacy.exists():
            for sub in legacy.iterdir():
                if sub.is_dir():
                    sources.append(("extracted_legacy", sub))

    return sources


def iter_text_files(root: Path):
    """Yield (rel_path, text) for every .txt under root."""
    for dirpath, _dirs, filenames in os.walk(root):
        for fn in filenames:
            if not fn.endswith(".txt"):
                continue
            abs_path = os.path.join(dirpath, fn)
            rel = os.path.relpath(abs_path, root).replace("\\", "/")
            try:
                text = Path(abs_path).read_text(encoding="utf-8", errors="replace")
            except Exception as e:
                print(f"  WARN: failed to read {rel}: {e}", file=sys.stderr)
                continue
            yield rel, text


def embed_batch(texts: list[str], model: str = EMBED_MODEL) -> list[list[float]]:
    """Call Ollama /api/embed. Returns list of vectors."""
    import urllib.request
    import json as _json
    req = urllib.request.Request(
        f"{OLLAMA_URL}/api/embed",
        data=_json.dumps({"model": model, "input": texts}).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            data = _json.loads(resp.read().decode("utf-8"))
        return data.get("embeddings", [])
    except Exception as e:
        print(f"  ERROR: Ollama embed failed: {e}", file=sys.stderr)
        return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--collection", default="important_docs",
                        help="ChromaDB collection name (default: important_docs)")
    parser.add_argument("--source", help="Custom source directory (default: auto-discover from .cache)")
    parser.add_argument("--chroma-dir", default=str(DEFAULT_CHROMA_DIR))
    parser.add_argument("--batch-size", type=int, default=32,
                        help="Embedding batch size (default: 32)")
    parser.add_argument("--chunk-size", type=int, default=CHUNK_SIZE,
                        help=f"Chunk size in tokens (default: {CHUNK_SIZE})")
    parser.add_argument("--chunk-overlap", type=int, default=CHUNK_OVERLAP,
                        help=f"Chunk overlap in tokens (default: {CHUNK_OVERLAP})")
    parser.add_argument("--stats", action="store_true", help="Show index stats, no build")
    parser.add_argument("--drop", action="store_true", help="Drop collection before rebuilding")
    parser.add_argument("--no-legacy", action="store_true",
                        help="Skip legacy AI_Studio/Reports/sme_importantdocs/extracted/")
    args = parser.parse_args()

    chroma_dir = Path(args.chroma_dir)
    chroma_dir.mkdir(parents=True, exist_ok=True)

    print(f"Collection: {args.collection}", file=sys.stderr)
    print(f"Chroma:     {chroma_dir}", file=sys.stderr)

    import chromadb
    client = chromadb.PersistentClient(path=str(chroma_dir))

    if args.drop:
        try:
            client.delete_collection(args.collection)
            print(f"Dropped existing collection '{args.collection}'", file=sys.stderr)
        except Exception:
            pass

    collection = client.get_or_create_collection(
        name=args.collection,
        metadata={"embed_model": EMBED_MODEL, "embed_dim": EMBED_DIM, "created": int(time.time())},
    )

    existing_count = collection.count()
    print(f"Existing chunks in collection: {existing_count}", file=sys.stderr)

    if args.stats:
        if existing_count > 0:
            # Show source breakdown
            all_meta = collection.get(include=["metadatas"], limit=10000)
            doc_types = {}
            for m in all_meta.get("metadatas", []):
                dt = m.get("doc_type", "unknown")
                doc_types[dt] = doc_types.get(dt, 0) + 1
            print("Chunks by doc_type:", file=sys.stderr)
            for dt, n in sorted(doc_types.items(), key=lambda x: -x[1]):
                print(f"  {dt}: {n}", file=sys.stderr)
        return 0

    # Discover sources
    if args.source:
        sources = [("custom", Path(args.source).resolve())]
    else:
        sources = discover_sources(include_legacy=not args.no_legacy)

    print(f"\nDiscovered {len(sources)} source directories:", file=sys.stderr)
    for doc_type, root in sources:
        n_files = sum(1 for _ in os.walk(root) for _ in _[2] if _[2])
        print(f"  [{doc_type}] {root}", file=sys.stderr)

    # Track existing IDs to skip duplicates
    existing_ids = set()
    if existing_count > 0:
        chunk_batch = 1000
        offset = 0
        while offset < existing_count:
            got = collection.get(limit=chunk_batch, offset=offset, include=[])
            existing_ids.update(got.get("ids", []))
            offset += chunk_batch

    # Build chunks
    to_embed = []  # list of {id, text, metadata}
    file_count = 0
    skip_count = 0
    empty_count = 0

    for doc_type, root in sources:
        for rel, text in iter_text_files(root):
            file_count += 1
            if not text or len(text.strip()) < 20:
                empty_count += 1
                continue
            chunks = chunk_text(text, args.chunk_size, args.chunk_overlap)
            for c in chunks:
                # Stable ID: hash of source root + rel + chunk idx
                raw = f"{root}:{rel}:{c['idx']}"
                chunk_id = hashlib.sha1(raw.encode("utf-8")).hexdigest()
                if chunk_id in existing_ids:
                    skip_count += 1
                    continue
                to_embed.append({
                    "id": chunk_id,
                    "text": c["text"],
                    "metadata": {
                        "doc_type": doc_type,
                        "source_root": str(root),
                        "rel_path": rel,
                        "chunk_idx": c["idx"],
                        "char_start": c["char_start"],
                        "char_end": c["char_end"],
                    },
                })

    print(f"\nFiles scanned:   {file_count}", file=sys.stderr)
    print(f"Empty/skipped:   {empty_count}", file=sys.stderr)
    print(f"Chunks ready:    {len(to_embed)}", file=sys.stderr)
    print(f"Already cached:  {skip_count}", file=sys.stderr)

    if not to_embed:
        print("Nothing new to embed.", file=sys.stderr)
        return 0

    # Embed in batches
    total = len(to_embed)
    t0 = time.perf_counter()
    added = 0
    failed = 0
    for i in range(0, total, args.batch_size):
        batch = to_embed[i:i + args.batch_size]
        texts = [c["text"] for c in batch]
        embeddings = embed_batch(texts)
        if len(embeddings) != len(batch):
            print(f"  [{i}/{total}] embed returned {len(embeddings)} vectors for {len(batch)} inputs — skipping batch", file=sys.stderr)
            failed += len(batch)
            continue
        collection.add(
            ids=[c["id"] for c in batch],
            embeddings=embeddings,
            documents=texts,
            metadatas=[c["metadata"] for c in batch],
        )
        added += len(batch)
        elapsed = time.perf_counter() - t0
        rate = added / elapsed if elapsed else 0
        pct = (i + len(batch)) * 100 // total
        print(f"  [{pct}%] {added}/{total} chunks ({rate:.1f}/s)", file=sys.stderr)

    total_elapsed = time.perf_counter() - t0
    print(f"\nAdded {added} chunks in {total_elapsed:.1f}s ({added/total_elapsed:.1f}/s)", file=sys.stderr)
    if failed:
        print(f"Failed: {failed} chunks", file=sys.stderr)
    print(f"Collection now has: {collection.count()} chunks", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    sys.exit(main())
