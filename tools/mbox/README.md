# tools/mbox — streaming mbox indexer + FTS search

Handles multi-GB Gmail Takeout exports. Built because the old `/mbox-parse`
slash command loaded everything into RAM and choked above ~1 GB.

## What it does

| Feature | Implementation |
|---|---|
| Streaming parse | Line-based mbox split in `store.iter_mbox` — no `mailbox.mbox` O(file) scan |
| Dedup | `UNIQUE(message_id)` on the messages table; synthetic ID fallback if missing |
| Attachment CAS | SHA256 → `attachments/<sha256[:2]>/<sha256>`, single copy of each unique file |
| Resumable | `ingest_progress` checkpoints; re-running skips completed mboxes, dedup handles partials cheaply |
| FTS | SQLite FTS5 with `trigram` tokenizer — substring search ("Warehm" hits "Wareham") and word search both work |
| In-memory feel | `PRAGMA mmap_size=30GB` + 2 GB page cache (128 GB RAM available) |

## Install

No external dependencies. Uses Python 3.10+ stdlib only (`sqlite3`, `email`,
`hashlib`, `html`, `pathlib`).

## Index

```bash
# Index everything in the default dir (C:/Users/atayl/Desktop/Excluded/mbox/)
python -m tools.mbox.index

# Index a specific directory
python -m tools.mbox.index "C:/Users/atayl/Desktop/Excluded/mbox"

# Index a single file
python -m tools.mbox.index "C:/Users/atayl/Desktop/Excluded/mbox/Legal.mbox"

# Skip attachments (much faster for first pass)
python -m tools.mbox.index --no-attachments

# Force reindex (ignore completed checkpoints)
python -m tools.mbox.index --force

# Show progress
python -m tools.mbox.index --status
```

Outputs:
- `C:/Users/atayl/Desktop/Excluded/mbox/mbox_index.db` — SQLite FTS5 index
- `C:/Users/atayl/Desktop/Excluded/mbox/attachments/<XX>/<sha256>` — CAS store

## Search

```bash
# Full-text (trigram) — substring works
python -m tools.mbox.search "NARSUM"
python -m tools.mbox.search Wareham

# Structured filters — combine freely
python -m tools.mbox.search --from tolin --since 2026-03-01
python -m tools.mbox.search --subject "privilege" --has-attachment
python -m tools.mbox.search --to adam.taylor --since 2026-01-01 --until 2026-04-01
python -m tools.mbox.search --label Legal "DCSA"
python -m tools.mbox.search --attachments "narsum*.pdf"

# Full message view
python -m tools.mbox.search --id 12345
python -m tools.mbox.search --message-id "<abc@example.com>"

# JSON output (for agent pipelines)
python -m tools.mbox.search "query" --json --limit 100

# Count only
python -m tools.mbox.search --from wareham --count
```

## Schema at a glance

- **messages** — one row per deduplicated email. Contains headers, body_text, source provenance.
- **messages_fts** — contentless FTS5 (trigram). Joined on `rowid = messages.id`.
- **attachments** — one row per unique SHA256. `stored_path` is the canonical on-disk location.
- **message_attachments** — many-to-many (same attachment in N messages).
- **attachments_fts** — optional FTS over extracted attachment text (populated by later OCR pass).
- **ingest_progress** — per-mbox checkpoint, resumable.

See `schema.sql` for full DDL.

## Design trade-offs

- **SQLite FTS5 over Tantivy/PostgreSQL/Elasticsearch**: stdlib, zero-deps, single file, good enough at this scale. With mmap + 128 GB RAM available, the whole index memory-maps without hacks. Abstraction is clean, so swap-in later is straightforward if needed.
- **Contentless FTS5**: avoids storing body text twice (once in messages, once in FTS). Join back to `messages` by rowid for display.
- **Trigram tokenizer only (no unicode61)**: substring + word search in one table. 2–3× slower than unicode61 for pure word queries, but queries are already <200 ms at this scale. Dual-FTS was rejected as premature optimization.
- **Body text capped at 4 MB per message**: practically unlimited, but defends against pathological messages.
- **`body_html_stripped=1` flag**: tracks which bodies were originally HTML for audit.

## Known limitations

- Mbox splitting is line-based on `^From `. Rare: unescaped `From ` in body → mis-split. Gmail exports are generally clean.
- Attachment content extraction (PDF/DOCX → searchable text) is not done here. Populate `attachments.extracted_text` later via a separate pass that reuses the CAS store.
- Message-ID normalization is lowercase + angle-bracket strip. Some legacy clients put weird things in Message-ID.
