---
description: "compaction instructions — CLAUDE.md integration, PreCompact hooks, analysis scratchpad, what to preserve, how to write effective instructions"
---

# Compaction Instructions -- Arcanum Wiki

## Overview

Claude Code's compaction system supports user-customizable instructions that influence what information survives compaction. These instructions can be provided through three mechanisms: a `## Compaction Instructions` section in CLAUDE.md, PreCompact hooks that inject dynamic instructions, and custom instructions passed to the `/compact` command. This article explains exactly how each mechanism works at the code level.

## How It Works

### The Compaction Prompt

The full compaction prompt is defined in `src/services/compact/prompt.ts`. It instructs the summarizer model to produce a structured summary with 9 sections: Primary Request and Intent, Key Technical Concepts, Files and Code Sections, Errors and Fixes, Problem Solving, All User Messages, Pending Tasks, Current Work, and Optional Next Step.

At the end of the standard prompt, a critical paragraph enables user customization:

```
There may be additional summarization instructions provided in the included context.
If so, remember to follow these instructions when creating the above summary.
Examples of instructions include:
## Compact Instructions
When summarizing the conversation focus on typescript code changes...
```

This means the compaction model actively looks for sections named `## Compact Instructions` or `## Compaction Instructions` in the loaded context (which includes CLAUDE.md content) and treats them as additional directives.

### CLAUDE.md Integration

Because CLAUDE.md content is part of the conversation context at compaction time, any section with the right heading is picked up automatically. The compaction model receives the full conversation history (including the first user message where CLAUDE.md is injected as a `<system-reminder>`) and will find and follow those instructions.

The VoxCore project uses this mechanism with:

```markdown
## Compaction Instructions
When compacting, ALWAYS preserve:
(1) files modified this session with full paths
(2) current task/goal and exact user request
(3) pending SQL or build actions
(4) spawned agents and their findings
Drop: exploration results, failed approaches, verbose tool output.
```

### PreCompact Hooks

The compaction system runs `executePreCompactHooks()` before generating the summary. PreCompact hooks can return `newCustomInstructions` which are injected into the compaction prompt alongside any CLAUDE.md instructions.

From `compact.ts`, the PreCompact hook integration:

```typescript
const preCompactResults = await executePreCompactHooks(...)
for (const result of preCompactResults) {
  if (result.hookSpecificOutput?.customInstructions) {
    customInstructions.push(result.hookSpecificOutput.customInstructions)
  }
}
```

This is more reliable than CLAUDE.md because hooks inject instructions directly into the compaction prompt rather than relying on the model finding them in context. For critical preservation requirements, a PreCompact hook is the recommended approach.

### Manual `/compact` Instructions

When users run `/compact` with custom text, that text is passed as `customInstructions` to the compaction function. It is injected into the prompt alongside hook-provided instructions.

### The Analysis Scratchpad

The compaction prompt uses an `<analysis>` block as a drafting scratchpad. The model is instructed to organize its thoughts inside `<analysis>` tags before writing the final `<summary>`. The `formatCompactSummary()` function then strips the `<analysis>` block before the summary enters the conversation context. This means the model has room to reason about what to preserve (including following custom instructions), but only the distilled `<summary>` content survives.

### Post-Compact Message Framing

The summary is wrapped in a framing message:

```
This session is being continued from a previous conversation that ran out of context.
The summary below covers the earlier portion of the conversation.

[formatted summary]

If you need specific details from before compaction (like exact code snippets,
error messages, or content you generated), read the full transcript at: [path]
```

For auto-compact (where `suppressFollowUpQuestions` is true), an additional directive is appended:

```
Continue the conversation from where it left off without asking the user any
further questions. Resume directly -- do not acknowledge the summary, do not
recap what was happening, do not preface with "I'll continue" or similar.
Pick up the last task as if the break never happened.
```

### Token Budget for Summaries

The compaction summary output is capped at 20,000 tokens (`MAX_OUTPUT_TOKENS_FOR_SUMMARY`). For a 167,000 token conversation, this represents approximately 88% compression. Custom instructions compete for space within this budget -- overly detailed preservation instructions may cause the model to truncate other sections.

### What Survives Without Instructions

Even without custom instructions, the default compaction prompt preserves:
- User requests and intent
- Technical concepts and decisions
- File names and code snippets
- Errors encountered and fixes applied
- Problem-solving approaches
- All user messages (non-tool-result)
- Pending tasks
- Current work state
- Suggested next steps

Custom instructions add specificity to what within these categories gets prioritized.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/services/compact/prompt.ts` | Compaction prompt templates, custom instruction injection |
| `src/services/compact/compact.ts` | PreCompact hook execution, summary formatting |
| `src/utils/hooks.ts` | PreCompact hook infrastructure |

## Configuration

To add compaction instructions, choose one of:

1. **CLAUDE.md** (simplest): Add a `## Compaction Instructions` section to your project's CLAUDE.md
2. **PreCompact hook** (most reliable): Add to `.claude/settings.json`:
   ```json
   {
     "hooks": {
       "PreCompact": [{
         "type": "command",
         "command": "echo 'Always preserve SQL file paths and database state'"
       }]
     }
   }
   ```
3. **Manual**: Run `/compact` with custom text when you want one-time instructions

## Cross-References

- [Compaction Overview](compaction_overview.md) -- System architecture
- [Compaction Tiers](compaction_tiers.md) -- Tier implementation details
- [System Prompt](system_prompt.md) -- How CLAUDE.md is injected into context
- [Hooks Overview](../hooks/overview.md) -- Hook system architecture

## Interesting Findings

**PostCompact hooks receive the summary.** After compaction completes, PostCompact hooks receive the generated `compactSummary` text and can display a `userDisplayMessage`. This enables workflows where a hook logs what was preserved or alerts the user about information loss.

**Partial compact prompts differ subtly.** The `up_to` direction prompt replaces section 8/9 with "Work Completed" and "Context for Continuing Work" (instead of "Current Work" and "Optional Next Step"), framing the summary as historical context rather than active state.

**The summary output cap creates an implicit priority.** At 20K tokens, the model must triage. Sections listed earlier in the prompt (Primary Request, Key Concepts) tend to get more space than later sections (Pending Tasks, Next Steps). Custom instructions that say "ALWAYS preserve X" effectively boost X's priority in this implicit ranking.
