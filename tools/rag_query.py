#!/usr/bin/env python3
"""Query the local RAG vector store built by tools/rag_build.py.

Natural-language queries over the IMPORTANT DOCS corpus. Embeds the query via
Ollama nomic-embed-text, runs vector similarity against ChromaDB, optionally
re-ranks results, and prints context-rich hits.

Usage:
  python tools/rag_query.py "who signed the final revocation?"
  python tools/rag_query.py "NPDB 30 day" --top 10
  python tools/rag_query.py "Aug 19 2024 meeting" --collection important_docs
  python tools/rag_query.py "ADSCD deadline" --show-text

Output modes:
  --format short    one line per hit: path | score | snippet
  --format full     full chunks with source path and score (default)
  --format json     JSON output for programmatic use

Filtering:
  --doc-type transcribed      only audio transcripts
  --doc-type extracted        only PDFs/DOCX/EML
  --doc-type ocr              only image OCR
  --rel-path-glob "*case*"    only paths matching glob

Requirements:
  pip install chromadb
  Ollama running, collection built via tools/rag_build.py
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHROMA_DIR = REPO_ROOT / ".cache" / "rag" / "chroma"
OLLAMA_URL = "http://localhost:11434"
EMBED_MODEL = "nomic-embed-text"


def embed_query(query: str) -> list[float] | None:
    import urllib.request
    import json as _json
    req = urllib.request.Request(
        f"{OLLAMA_URL}/api/embed",
        data=_json.dumps({"model": EMBED_MODEL, "input": query}).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = _json.loads(resp.read().decode("utf-8"))
        embeddings = data.get("embeddings", [])
        return embeddings[0] if embeddings else None
    except Exception as e:
        print(f"ERROR: Ollama embed failed: {e}", file=sys.stderr)
        return None


def format_hit_short(hit: dict, i: int) -> str:
    meta = hit["metadata"]
    snippet = hit["document"][:100].replace("\n", " ")
    return f"[{i}] {meta.get('doc_type', '?')} | {meta.get('rel_path', '?')} | score={hit['score']:.3f}\n    {snippet}..."


def format_hit_full(hit: dict, i: int) -> str:
    meta = hit["metadata"]
    parts = [
        f"--- Hit {i} ---",
        f"doc_type:  {meta.get('doc_type', '?')}",
        f"rel_path:  {meta.get('rel_path', '?')}",
        f"chunk_idx: {meta.get('chunk_idx', '?')}",
        f"char_span: {meta.get('char_start', 0)}-{meta.get('char_end', 0)}",
        f"score:     {hit['score']:.4f}",
        "",
        hit["document"],
        "",
    ]
    return "\n".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("query", nargs="+", help="Query text (can be unquoted multi-word)")
    parser.add_argument("--collection", default="important_docs", help="ChromaDB collection name")
    parser.add_argument("--chroma-dir", default=str(DEFAULT_CHROMA_DIR))
    parser.add_argument("--top", type=int, default=5, help="Number of results (default: 5)")
    parser.add_argument("--format", choices=["short", "full", "json"], default="full")
    parser.add_argument("--doc-type", choices=["extracted", "transcribed", "ocr", "extracted_legacy"],
                        help="Filter by document type")
    parser.add_argument("--rel-path-glob", help="Filter by rel_path substring match")
    args = parser.parse_args()

    query_text = " ".join(args.query).strip()
    if not query_text:
        print("ERROR: empty query", file=sys.stderr)
        return 1

    chroma_dir = Path(args.chroma_dir)
    if not chroma_dir.exists():
        print(f"ERROR: chroma dir {chroma_dir} does not exist. Run tools/rag_build.py first.", file=sys.stderr)
        return 1

    import chromadb
    client = chromadb.PersistentClient(path=str(chroma_dir))
    try:
        collection = client.get_collection(args.collection)
    except Exception as e:
        print(f"ERROR: collection '{args.collection}' not found: {e}", file=sys.stderr)
        return 1

    if args.format != "json":
        print(f"Query:      {query_text}", file=sys.stderr)
        print(f"Collection: {args.collection} ({collection.count()} chunks)", file=sys.stderr)

    t0 = time.perf_counter()
    query_vec = embed_query(query_text)
    if query_vec is None:
        return 1
    embed_time = time.perf_counter() - t0

    # Build ChromaDB where filter
    where = None
    if args.doc_type:
        where = {"doc_type": args.doc_type}

    # Over-fetch for substring filtering (rel_path_glob is client-side)
    n_request = args.top * 5 if args.rel_path_glob else args.top

    t1 = time.perf_counter()
    results = collection.query(
        query_embeddings=[query_vec],
        n_results=n_request,
        where=where,
        include=["documents", "metadatas", "distances"],
    )
    query_time = time.perf_counter() - t1

    hits = []
    docs = results.get("documents", [[]])[0]
    metas = results.get("metadatas", [[]])[0]
    dists = results.get("distances", [[]])[0]
    for doc, meta, dist in zip(docs, metas, dists):
        # ChromaDB returns L2 distance by default; convert to similarity
        score = 1.0 / (1.0 + dist)
        if args.rel_path_glob:
            rp = meta.get("rel_path", "")
            if args.rel_path_glob.lower() not in rp.lower():
                continue
        hits.append({"document": doc, "metadata": meta, "distance": dist, "score": score})
        if len(hits) >= args.top:
            break

    if args.format == "json":
        print(json.dumps({
            "query": query_text,
            "collection": args.collection,
            "count": len(hits),
            "embed_sec": round(embed_time, 3),
            "query_sec": round(query_time, 3),
            "hits": hits,
        }, indent=2, ensure_ascii=False))
        return 0

    print(f"Embed: {embed_time*1000:.0f}ms, Query: {query_time*1000:.0f}ms, Hits: {len(hits)}", file=sys.stderr)
    print(file=sys.stderr)

    for i, hit in enumerate(hits, 1):
        if args.format == "short":
            print(format_hit_short(hit, i))
        else:
            print(format_hit_full(hit, i))

    return 0


if __name__ == "__main__":
    sys.exit(main())
