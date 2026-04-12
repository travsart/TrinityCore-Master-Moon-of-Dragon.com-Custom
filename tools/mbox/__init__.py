"""VoxCore mbox indexing + search package.

Handles large Gmail Takeout exports (multi-GB) via streaming parse into a
SQLite FTS5 database with deduplication, content-addressable attachment
storage, and resumable checkpoints.

Modules:
    schema   — SQL DDL (embedded as a constant)
    index    — streaming indexer (python -m tools.mbox.index <mbox_dir>)
    search   — FTS5 query CLI      (python -m tools.mbox.search <query>)
    store    — shared utilities    (DB connect, CAS helpers, text extraction)
"""

__all__ = ["index", "search", "store"]
