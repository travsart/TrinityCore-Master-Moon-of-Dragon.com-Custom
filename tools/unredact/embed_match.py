"""Embedding-based semantic source matcher.

For each page in a redacted PDF that has some recoverable text, query the
ChromaDB RAG index (built by tools/rag_build.py) via nomic-embed-text
embeddings to find semantically-similar documents. This is a stronger
version of tools.unredact.reconstruct's FTS5-based matcher because it
handles paraphrase, reordering, and synonyms.

Pairs with the existing /rag-search slash command and the 27 new audio
transcripts that were just embedded.

Usage:
    python -m tools.unredact.embed_match <pdf>
    python -m tools.unredact.embed_match <pdf> --page 234
    python -m tools.unredact.embed_match <pdf> --top 5
    python -m tools.unredact.embed_match <pdf> --doc-type transcribed

Requires:
    * Ollama running at localhost:11434 with nomic-embed-text pulled
    * ChromaDB persistent store at .cache/rag/chroma/ (built by rag_build.py)
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CHROMA_DIR = REPO_ROOT / ".cache" / "rag" / "chroma"
DEFAULT_COLLECTION = "important_docs"
OLLAMA_EMBED_URL = "http://localhost:11434/api/embed"
EMBED_MODEL = "nomic-embed-text"
DEFAULT_OUTPUT = Path(r"C:\Users\atayl\Desktop\Excluded\unredact\embed_match")


def ollama_embed(text: str, model: str = EMBED_MODEL, timeout: int = 60) -> list[float]:
    """Embed a single string via Ollama and return the vector."""
    payload = {"model": model, "input": text}
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        OLLAMA_EMBED_URL, data=data,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = json.loads(resp.read().decode("utf-8"))
    # Ollama returns either {"embeddings": [[...]]} or {"embedding": [...]}
    if "embeddings" in body:
        embs = body["embeddings"]
        return embs[0] if embs else []
    return body.get("embedding", [])


def chunk_for_query(text: str, max_len: int = 3000) -> str:
    """Normalize and cap a page's text before embedding."""
    text = " ".join(text.split())
    return text[:max_len]


def connect_chroma():
    try:
        import chromadb
    except ImportError:
        print("ERROR: chromadb not installed. pip install chromadb", file=sys.stderr)
        return None
    try:
        client = chromadb.PersistentClient(path=str(DEFAULT_CHROMA_DIR))
        return client
    except Exception as e:
        print(f"ERROR: ChromaDB client failed: {e}", file=sys.stderr)
        return None


def get_collection(client, name: str = DEFAULT_COLLECTION):
    try:
        return client.get_collection(name=name)
    except Exception as e:
        print(f"ERROR: collection '{name}' not found: {e}", file=sys.stderr)
        print(f"Build it first: python tools/rag_build.py", file=sys.stderr)
        return None


def query_collection(collection, query_embedding: list[float],
                     top_k: int = 5, doc_type_filter: str | None = None) -> list[dict]:
    """Run a vector-similarity query."""
    where = {"doc_type": doc_type_filter} if doc_type_filter else None
    try:
        result = collection.query(
            query_embeddings=[query_embedding],
            n_results=top_k,
            where=where,
        )
    except Exception as e:
        return [{"error": str(e)}]

    # Chroma returns a dict with lists — one per query (we sent 1)
    ids = (result.get("ids") or [[]])[0]
    distances = (result.get("distances") or [[]])[0]
    metadatas = (result.get("metadatas") or [[]])[0]
    documents = (result.get("documents") or [[]])[0]

    hits: list[dict] = []
    for i in range(len(ids)):
        md = metadatas[i] if i < len(metadatas) else {}
        hits.append({
            "id": ids[i],
            "distance": distances[i] if i < len(distances) else None,
            "similarity": (1.0 - distances[i]) if i < len(distances) and distances[i] is not None else None,
            "doc_type": (md or {}).get("doc_type"),
            "rel_path": (md or {}).get("rel_path"),
            "chunk_idx": (md or {}).get("chunk_idx"),
            "text": (documents[i] if i < len(documents) else "") or "",
        })
    return hits


def process_pdf(pdf_path: Path, output_dir: Path, top_k: int = 5,
                only_page: int | None = None,
                doc_type: str | None = None,
                collection_name: str = DEFAULT_COLLECTION) -> dict:
    import fitz

    output_dir.mkdir(parents=True, exist_ok=True)
    client = connect_chroma()
    if client is None:
        return {"error": "ChromaDB unavailable"}
    collection = get_collection(client, collection_name)
    if collection is None:
        return {"error": "collection unavailable"}

    doc = fitz.open(str(pdf_path))
    total_pages = len(doc)
    results: list[dict] = []
    t0 = time.time()
    pages_queried = 0

    try:
        for i in range(total_pages):
            if only_page is not None and (i + 1) != only_page:
                continue
            page = doc[i]
            text = (page.get_text("text") or "").strip()
            if len(text) < 200:
                continue
            query = chunk_for_query(text)
            print(f"  [page {i+1}] embedding {len(query)} chars...", end=" ")
            try:
                emb = ollama_embed(query)
            except urllib.error.URLError as e:
                print(f"embed failed: {e}")
                continue
            except Exception as e:
                print(f"embed failed: {e}")
                continue
            if not emb:
                print("empty embedding")
                continue

            hits = query_collection(collection, emb, top_k=top_k, doc_type_filter=doc_type)
            err_hit = next((h for h in hits if "error" in h), None)
            if err_hit:
                print(f"query error: {err_hit['error']}")
                continue
            pages_queried += 1
            best = hits[0] if hits else None
            if best:
                sim = best.get("similarity")
                print(f"top sim={sim:.3f}" if sim is not None else "top hit")
            results.append({
                "page": i + 1,
                "query_preview": query[:200],
                "hits": hits,
            })
    finally:
        doc.close()

    elapsed_total = time.time() - t0

    lines = [
        f"# Embedding-based source matches: `{pdf_path.name}`",
        "",
        f"- Collection:  {collection_name}",
        f"- Embed model: {EMBED_MODEL}",
        f"- Pages queried: {pages_queried}",
        f"- Elapsed:     {elapsed_total:.1f}s",
        "",
        "---",
        "",
    ]

    for r in results:
        lines.append(f"## Page {r['page']}")
        lines.append("")
        lines.append(f"**Query preview**: `{r['query_preview'][:200]}`")
        lines.append("")
        for i, h in enumerate(r["hits"], 1):
            sim = h.get("similarity")
            sim_str = f"{sim:.3f}" if sim is not None else "?"
            lines.append(
                f"{i}. **sim {sim_str}** · `{h.get('doc_type', '?')}` · "
                f"`{h.get('rel_path', '?')}`  (chunk {h.get('chunk_idx', '?')})"
            )
            preview = (h.get("text") or "")[:300].replace("\n", " ")
            lines.append(f"   > {preview}{'...' if len(h.get('text') or '') > 300 else ''}")
            lines.append("")

    out_path = output_dir / f"{pdf_path.stem}.embed_match.md"
    out_path.write_text("\n".join(lines), encoding="utf-8")

    return {
        "pdf": str(pdf_path),
        "pages_queried": pages_queried,
        "elapsed": elapsed_total,
        "report": str(out_path),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.unredact.embed_match")
    p.add_argument("pdf")
    p.add_argument("--output", default=str(DEFAULT_OUTPUT))
    p.add_argument("--top", type=int, default=5)
    p.add_argument("--page", type=int, default=None)
    p.add_argument("--doc-type", default=None,
                   help="Filter by doc_type metadata: extracted | transcribed | ocr")
    p.add_argument("--collection", default=DEFAULT_COLLECTION)
    args = p.parse_args(argv)

    pdf = Path(args.pdf).resolve()
    if not pdf.exists():
        print(f"ERROR: {pdf} not found", file=sys.stderr)
        return 2

    output_dir = Path(args.output).resolve()
    print(f"PDF:        {pdf}")
    print(f"Collection: {args.collection}")
    print(f"Output:     {output_dir}")
    print()

    r = process_pdf(pdf, output_dir, top_k=args.top,
                    only_page=args.page,
                    doc_type=args.doc_type,
                    collection_name=args.collection)
    if "error" in r:
        print(f"ERROR: {r['error']}", file=sys.stderr)
        return 3
    print()
    print("=" * 60)
    print(f"Done in {r['elapsed']:.1f}s, {r['pages_queried']} pages queried")
    print(f"Report: {r['report']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
