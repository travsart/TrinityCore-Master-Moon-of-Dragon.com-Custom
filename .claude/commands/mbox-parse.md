---
allowed-tools: Bash(python3:*), Bash(python:*), Read, Write, Grep, Glob, Agent
description: Stream-index and search large Gmail .mbox exports (multi-GB) — SQLite FTS5, dedup, CAS attachments
---

# Mbox Parser (streaming + FTS5)

Handle multi-GB Gmail Takeout `.mbox` exports. Builds a SQLite FTS5 index with
dedup, content-addressable attachment storage, and resumable checkpoints.

Implementation lives in `tools/mbox/` — see `tools/mbox/README.md` for the
full design. This slash command is a thin wrapper that routes the user's
intent to the right sub-command.

## Arguments

The user provides one of:
- Nothing → show status of the default index
- A path to a `.mbox` file or directory → index it
- A command: `index`, `search <query>`, `show <id>`, `status`, `stats`
- Structured filter flags: `--from`, `--to`, `--subject`, `--label`,
  `--since`, `--until`, `--attachments`, `--has-attachment`

## Default Locations

- Mbox source:  `C:/Users/atayl/Desktop/Excluded/mbox/`
- SQLite index: `C:/Users/atayl/Desktop/Excluded/mbox/mbox_index.db`
- Attachment CAS: `C:/Users/atayl/Desktop/Excluded/mbox/attachments/<XX>/<sha256>`

## Commands

### `status` (default when no args)

```bash
python -m tools.mbox.index --status
```

Shows per-mbox progress, total unique messages, unique attachments, DB size.

### `index` (when a path is given)

```bash
# Default: index the whole mbox directory
python -m tools.mbox.index

# Index a specific file or dir
python -m tools.mbox.index "C:/Users/atayl/Desktop/Excluded/mbox/Legal.mbox"

# Skip attachment extraction (much faster for first pass)
python -m tools.mbox.index --no-attachments

# Force reindex (ignore completed checkpoints)
python -m tools.mbox.index --force
```

Resumable: completed mboxes are skipped; partial mboxes restart from 0 but
dedup on Message-ID makes the re-scan cheap.

### `search <query>` (full-text)

```bash
# Trigram tokenizer — substring AND word match both work
python -m tools.mbox.search "NARSUM"
python -m tools.mbox.search Wareham
python -m tools.mbox.search "Wheeler DCSA"
```

### Structured filters (combine freely)

```bash
python -m tools.mbox.search --from tolin --since 2026-03-01
python -m tools.mbox.search --subject "privilege" --has-attachment
python -m tools.mbox.search --to adam.taylor --since 2026-01-01 --until 2026-04-01
python -m tools.mbox.search --label Legal "DCSA"
python -m tools.mbox.search --attachments "narsum*.pdf"
```

### `show <id>` (single message)

```bash
python -m tools.mbox.search --id 12345
python -m tools.mbox.search --message-id "<abc@example.com>"
```

### JSON output (for agent pipelines)

```bash
python -m tools.mbox.search "Wheeler" --json --limit 100
```

## How to invoke from Claude Code

1. Run the command that matches user intent (from the table above).
2. Parse the output and summarise to the user.
3. For case evidence work, key search names: Wheeler, Campbell, Wiley,
   Wareham, Tolin, Ko, Iandoli, Earles, McMaster, Johnston, Garro,
   Corpening, Stringer, Gebhardt, Burns, Fain.
4. Key topics: QAI, IG, SAPR, SAPRO, DCSA, privilege, NARSUM, MEB, IDES,
   NPDB, AFBCMR, DD149, 1034.

## MCP alternative (arcanum)

The arcanum MCP server also exposes the mbox DB:

- `arcanum_mbox_search(query, sender, recipient, subject, label, since, until, has_attachment, attachment)`
- `arcanum_mbox_read(message_id)`

These call the same SQLite DB and are preferred from within a tool-use turn
because they return structured markdown ready to render.

## Important Notes

- First-time index of the full 9.8 GB set takes ~30–60 min single-threaded.
  Run it in the background (`run_in_background=true`).
- The old `mbox_index.json` / `mbox_summary.md` outputs are gone — the
  SQLite DB replaces them. If you need a JSON dump for legacy consumers,
  use `python -m tools.mbox.search --json` with appropriate filters.
- Gmail exports overlap heavily (same message in Inbox + Sent + Important
  + Opened + Category_*). Dedup by Message-ID typically collapses 2–3×.
- Existing curated evidence in `Case_Reference/11_EMAILS/` (689 emails) is
  separate — use that for authoritative evidence, use the mbox index for
  discovery across the full archive.
