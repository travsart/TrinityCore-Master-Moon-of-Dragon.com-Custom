---
description: "MagicDocs service — automatic documentation generation, context-aware doc suggestions, inline documentation assistance"
---

# MagicDocs Service -- Arcanum Wiki

## What Is This?

MagicDocs is an automatic documentation maintenance system. When Claude Code reads a file whose first line matches `# MAGIC DOC: [title]`, that file is registered as a "Magic Doc." After each conversation turn (when the model is idle), a background subagent updates all tracked Magic Docs with new learnings from the conversation. Currently Ant-only.

## How It Works

**Detection:** A file read listener registered on `FileReadTool` scans every file read for the pattern `^# MAGIC DOC: [title]`. An optional italicized line immediately after the header is captured as custom instructions for that document.

**Tracking:** Detected docs are stored in a `Map<string, MagicDocInfo>`. Each file is registered only once per session. If a doc's header is removed or the file is deleted, it is unregistered on the next update cycle.

**Update cycle:** A post-sampling hook fires after each main REPL thread response. It only runs when:
- The query source is `repl_main_thread`
- No tool calls exist in the last assistant turn (conversation is "idle")
- At least one Magic Doc is tracked

For each tracked doc, the service:
1. Clones the `FileStateCache` and deletes the doc's entry (forces fresh read)
2. Re-reads the file to verify the Magic Doc header still exists
3. Builds an update prompt with the current content, title, and instructions
4. Runs `runAgent()` with a `magic-docs` agent definition (uses Sonnet, Edit-only tools)
5. The agent can only edit the exact Magic Doc file path

**Update philosophy:** The prompt instructs the agent to maintain the document as a living overview -- update in-place, remove outdated info, fix errors, and focus on architecture/entry points/patterns rather than exhaustive code walkthroughs.

## Key Source Files

| File | Purpose |
|------|---------|
| `magicDocs.ts` | Detection, tracking, update orchestration |
| `prompts.ts` | Update prompt template with variable substitution |

## Configuration

- Ant-only (gated by `process.env.USER_TYPE === 'ant'`)
- Custom prompts: `~/.claude/magic-docs/prompt.md`
- Uses `{{docContents}}`, `{{docPath}}`, `{{docTitle}}`, `{{customInstructions}}` template variables

## Interesting Findings

1. **Magic Docs piggyback on the existing agent infrastructure** -- they use `runAgent()` with a purpose-built `BuiltInAgentDefinition` rather than the forked agent pattern used by session memory and extract memories.

2. **The update is sequential** -- all tracked docs are updated one at a time via `sequential()`, not in parallel.

3. **Custom per-doc instructions** are extracted from the italicized line under the header. These take priority over the general update rules, allowing each doc to specify its own update behavior.

4. **Files that lose their header are automatically untracked.** The re-read step on each update cycle verifies the header still exists, providing self-cleaning behavior.
