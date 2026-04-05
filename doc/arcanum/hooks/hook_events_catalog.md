---
description: "hook events catalog — all 27 hook event types, PreToolUse PostToolUse UserPromptSubmit Stop Notification Compact, trigger conditions"
---

# Hook Events Catalog
> Source: `src/hooks/` (104 files)
> Status: STUB — needs research

## What This Covers
Complete catalog of all 27 hook events with their parameters, timing, and use cases.

## Known Events (from Tier 2 report)
PreToolUse, PostToolUse, Notification, Stop, SubagentStart, ConfigChange, plus 21 more.

## Key Questions
- Full list of all 27 events with parameter schemas
- Which events are pre-execution (can block) vs post-execution (observe only)?
- Event ordering when multiple hooks match
- Which events are available in current live version vs source-only?
