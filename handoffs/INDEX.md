# Handoff Index

_Auto-maintained by `/handoff` (append per new handoff) and `/handoff-index` (full rebuild from disk)._
_Last rebuild: 2026-04-12 (seeded)._

The canonical storage for handoffs is **per-project** at `<project>/AI_Studio/Handoffs/YYYY-MM-DD_<topic>.md`. Each handoff is hardlinked to `~/Desktop/AI_Handoffs/<project>/<same-filename>.md` for human eyeballing. This file is the aggregation layer — a cross-project data table for insights, gap analysis, and hygiene queries.

## Active Handoffs (newest first)

| Date | Project | Topic | Branch | Last Commit | Size | Path |
|------|---------|-------|--------|-------------|------|------|
| 2026-04-12 | calmcore | handoff-architecture-v1 | master | 5952cd3316 | 10.3 KB | [open](../../CalmCore/AI_Studio/Handoffs/2026-04-12_handoff-architecture-v1.md) |
| 2026-04-12 | calmsniffer | aes-gcm-decrypt | master | 258b79c | 6.0 KB | [open](../../CalmSniffer/AI_Studio/Handoffs/2026-04-12_aes-gcm-decrypt.md) |

## Project Activity Summary

| Project | Handoff Count | Oldest | Newest | Days Since Last |
|---------|---------------|--------|--------|-----------------|
| calmcore | 1 | 2026-04-12 | 2026-04-12 | 0 |
| voxcore | 0 | — | — | — |
| calmsniffer | 1 | 2026-04-12 | 2026-04-12 | 0 |

## Hygiene Warnings

_(none)_

## Insights / Gap Analysis / Hygiene

These three folders hold post-synthesis work — patterns across multiple handoffs, questions the handoff corpus is NOT answering, and hygiene audits. They are manually curated; `/handoff-index` does not modify their contents.

- `_insights/` — cross-handoff patterns, recurring bug classes, architectural observations
- `_gap-analysis/` — what handoffs are NOT capturing; blind spots; missing coverage
- `_hygiene/` — drift detection, naming inconsistencies, stale-handoff rollups

## Conventions

**Filename**: `YYYY-MM-DD_<kebab-case-topic>.md`. Date must be ISO (not regional). Topic should be specific enough to disambiguate without needing the full file — `ai-audit-pass2` beats `fixes`.

**Storage**: Canonical file lives with the project it concerns. Projects outside `calmcore`/`voxcore`/`calmsniffer` require explicit confirmation in `/handoff` before a new `Desktop/AI_Handoffs/<project>/` folder is created.

**Hardlinks**: `ln <canonical> <desktop-link>` (Git Bash). Same-volume NTFS only. Editing either location updates both. No duplication.

**Aggregation**: Every `/handoff` invocation appends a row to this file. `/handoff-index` regenerates the table from scratch to fix drift (moved files, renamed projects, etc.).
