-- VoxCore mbox index schema.
--
-- Design decisions:
--   * SQLite + FTS5 (stdlib, single file, no server dependency)
--   * Contentless FTS5 with trigram tokenizer (substring/partial-name search)
--   * Aggressive mmap (128 GB RAM available) set at connection time, not here
--   * External CAS attachment store (attachments/<sha256[:2]>/<sha256>)
--   * Dedup by Message-ID primary, body_sha256 fallback
--   * Resumable per-mbox checkpoints in ingest_progress

-- Runtime pragmas are set by tools/mbox/store.py (page_size must be set on
-- empty DB, journal_mode=WAL is persistent, others are per-connection).

-- =========================================================================
-- messages — core row per deduplicated email
-- =========================================================================
CREATE TABLE IF NOT EXISTS messages (
    id               INTEGER PRIMARY KEY,
    message_id       TEXT UNIQUE NOT NULL,   -- RFC Message-ID or synthetic (sha256 of body+from+date)
    body_sha256      TEXT,                    -- hash of body text (secondary dedup signal)
    source_mbox      TEXT NOT NULL,           -- first mbox file where we saw this message
    source_offset    INTEGER,                 -- byte offset in that mbox (approximate; for provenance)
    date_sent        INTEGER,                 -- unix timestamp (NULL if Date header unparseable)
    date_raw         TEXT,                    -- raw Date: header for audit
    from_addr        TEXT,                    -- normalized email (lowercased)
    from_name        TEXT,                    -- display name
    to_addrs         TEXT,                    -- comma-joined recipient emails
    cc_addrs         TEXT,                    -- comma-joined cc emails
    subject          TEXT,
    in_reply_to      TEXT,                    -- threading
    references_hdr   TEXT,                    -- full References: header
    labels           TEXT,                    -- X-Gmail-Labels (comma-joined)
    size_bytes       INTEGER,                 -- approx raw RFC822 size
    attachment_count INTEGER DEFAULT 0,
    has_body         INTEGER DEFAULT 0,       -- 1 if body_text present, 0 if empty
    body_text        TEXT,                    -- plain-text body (after HTML strip if needed)
    body_html_stripped INTEGER DEFAULT 0      -- 1 if body was originally HTML and we stripped it
);

CREATE INDEX IF NOT EXISTS idx_messages_date      ON messages(date_sent);
CREATE INDEX IF NOT EXISTS idx_messages_from      ON messages(from_addr);
CREATE INDEX IF NOT EXISTS idx_messages_subject   ON messages(subject);
CREATE INDEX IF NOT EXISTS idx_messages_inreply   ON messages(in_reply_to);
CREATE INDEX IF NOT EXISTS idx_messages_bodyhash  ON messages(body_sha256);

-- =========================================================================
-- messages_fts — contentless FTS5, trigram tokenizer
--
-- rowid is set equal to messages.id on insert; joins go messages_fts.rowid
-- = messages.id. Trigram tokenizer enables substring match ("Warehm" will
-- hit "Wareham") and still handles full-word queries acceptably.
-- =========================================================================
CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5(
    subject,
    from_addr,
    from_name,
    to_addrs,
    body,
    content='',
    tokenize='trigram'
);

-- =========================================================================
-- attachments — content-addressable store (one row per unique sha256)
-- =========================================================================
CREATE TABLE IF NOT EXISTS attachments (
    sha256          TEXT PRIMARY KEY,
    filename        TEXT,              -- first-seen filename (canonical)
    mime_type       TEXT,
    size_bytes      INTEGER,
    stored_path     TEXT,              -- absolute path on disk, or NULL if not extracted
    extracted_text  TEXT,              -- optional OCR/text extraction (populated later)
    first_seen_ts   INTEGER
);

CREATE INDEX IF NOT EXISTS idx_attachments_filename ON attachments(filename);
CREATE INDEX IF NOT EXISTS idx_attachments_mime     ON attachments(mime_type);

-- =========================================================================
-- message_attachments — many-to-many link (same file attached to N emails)
-- =========================================================================
CREATE TABLE IF NOT EXISTS message_attachments (
    message_id         INTEGER NOT NULL,
    attachment_sha256  TEXT NOT NULL,
    filename_as_sent   TEXT,            -- filename in THIS message (may differ from canonical)
    PRIMARY KEY (message_id, attachment_sha256, filename_as_sent),
    FOREIGN KEY (message_id)        REFERENCES messages(id),
    FOREIGN KEY (attachment_sha256) REFERENCES attachments(sha256)
);

CREATE INDEX IF NOT EXISTS idx_msgatt_message ON message_attachments(message_id);
CREATE INDEX IF NOT EXISTS idx_msgatt_att     ON message_attachments(attachment_sha256);

-- =========================================================================
-- attachments_fts — optional FTS over extracted attachment text
-- =========================================================================
CREATE VIRTUAL TABLE IF NOT EXISTS attachments_fts USING fts5(
    filename,
    extracted_text,
    content='',
    tokenize='trigram'
);

-- =========================================================================
-- ingest_progress — resumable checkpoint per source mbox
-- =========================================================================
CREATE TABLE IF NOT EXISTS ingest_progress (
    mbox_file              TEXT PRIMARY KEY,   -- absolute path
    size_bytes             INTEGER,
    last_byte_offset       INTEGER DEFAULT 0,  -- for informational / skip logic
    messages_processed     INTEGER DEFAULT 0,  -- total messages read from this mbox
    messages_new           INTEGER DEFAULT 0,  -- inserted (unique Message-ID)
    messages_duplicate     INTEGER DEFAULT 0,  -- skipped due to dedup
    attachments_extracted  INTEGER DEFAULT 0,  -- new unique sha256 stored
    attachments_deduped    INTEGER DEFAULT 0,  -- matched existing sha256
    started_at             INTEGER,
    updated_at             INTEGER,
    completed_at           INTEGER,            -- NULL = still in progress
    error_count            INTEGER DEFAULT 0,
    last_error             TEXT
);

-- =========================================================================
-- meta — global key/value store (schema version, global stats, config)
-- =========================================================================
CREATE TABLE IF NOT EXISTS meta (
    key    TEXT PRIMARY KEY,
    value  TEXT
);

INSERT OR IGNORE INTO meta(key, value) VALUES ('schema_version', '1');
INSERT OR IGNORE INTO meta(key, value) VALUES ('created_by', 'tools/mbox/index.py');
