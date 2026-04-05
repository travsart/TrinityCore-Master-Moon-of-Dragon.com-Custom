---
description: "top 15 discoveries — CLAUDE.md second-class, blind memory selector, speculation, 50K tool limit, hook allow deny, fork cache, 1M free, git 2K cap, YOLO denials"
---

# Guide: What We Learned — The 15 Most Important Discoveries — Arcanum Wiki

> A synthesis of the most impactful findings from reverse-engineering Claude Code's source.

## The 15 Discoveries That Change How You Use Claude Code

### 1. CLAUDE.md Is Second-Class Content

**The finding**: CLAUDE.md is NOT in the API system prompt. It's injected as the first user message with `<system-reminder>` tags and a disclaimer: "may or may not be relevant."

**Impact**: The model treats system prompt instructions as higher authority. Your CLAUDE.md rules compete with Claude's built-in personality, not override it.

**Exploit**: Use emphatic language (MUST, ALWAYS, NEVER, CRITICAL). Put most important rules first. Keep it short and focused.

### 2. The Memory Selector Is Blind

**The finding**: The Sonnet side-query that picks which memory files to load ONLY sees filenames and `description` frontmatter. It never reads content during selection.

**Impact**: A brilliant 500-line memory file with a bad filename and no description will never be loaded.

**Exploit**: Write keyword-rich descriptions. Use descriptive filenames. The filename IS metadata.

### 3. Speculation Pre-Runs Your Tools

**The finding**: Claude Code has a speculative execution system that predicts tool calls and pre-runs them during response streaming.

**Impact**: Tool calls sometimes return "instantly" because they were pre-run. This is why Claude Code feels faster than API latency alone.

**Exploit**: Nothing to exploit — just know it's happening. Read-heavy workflows benefit most.

### 4. The 50K Character Tool Result Limit Is Your Friend

**The finding**: Tool results >50K chars are persisted to disk and replaced with previews. Per-message aggregate cap is 200K chars.

**Impact**: Reading a 10K line file doesn't consume 40K tokens of context. The system automatically manages this.

**Exploit**: Use Read's `offset`/`limit` parameters to get exactly what you need. Don't fight the limit — it's protecting your context.

### 5. Hook "allow" Cannot Override "deny"

**The finding**: The permission system has 8 rule sources. `allow` from one source NEVER overrides `deny` from another. Most restrictive wins.

**Impact**: You can't use hooks to bypass deny rules. Defense-in-depth is baked in.

**Exploit**: Use deny rules as absolute safety nets (they can't be circumvented). Use allow rules for convenience (they can be overridden by deny).

### 6. Fork Subagents Share Prompt Cache

**The finding**: Fork-mode subagents start with the parent's exact prompt prefix. This triggers Anthropic's prompt caching.

**Impact**: Spawning 5 parallel Explore agents costs ~1.2x one agent, not 5x. The shared prefix is cached.

**Exploit**: Use parallel agents aggressively. The marginal cost of additional agents is low.

### 7. 1M Context Is Free on Max Plan

**The finding**: The `[1m]` suffix is client-side only — stripped before the API call, replaced with a beta header. Auto-compact threshold moves from 167K to 967K.

**Impact**: Most sessions will never hit compaction with 1M context. It's a massive quality-of-life improvement.

**Exploit**: Enable it. There's no reason not to: `claude-opus-4-6[1m]`

### 8. Git Status Is Capped at 2K Characters

**The finding**: The git status injected into context is hard-capped at 2,000 characters.

**Impact**: If you have 200 untracked files, git status is truncated and useless. Every session starts with blind spots.

**Exploit**: Maintain .gitignore religiously. A clean git status means the 2K budget shows meaningful changes.

### 9. AutoDream Consolidates Memory in Background

**The finding**: After 24h + 5 sessions, a 4-phase background process (Orient → Gather → Consolidate → Prune) updates your memory files.

**Impact**: Memory files may change between sessions without you touching them. This is generally helpful but can occasionally alter things you didn't expect.

**Exploit**: Know it exists. If memory files seem different, AutoDream may have run. It's gated by `tengu_onyx_plover`.

### 10. There Are 99 Slash Commands (18 Are Stubbed)

**The finding**: 84 command directories + 15 standalone files = 99 command sources. 18 are stubbed/dead.

**Impact**: Many useful commands aren't in `/help`. Hidden aliases exist.

**Exploit**: Try: `/checkpoint` (rewind), `/continue` (resume), `/fork` (branch), `/ctx_viz` (context visualizer), `/doctor` (health check).

### 11. The YOLO Classifier Tracks Denials

**The finding**: In auto mode, after several consecutive permission denials, the classifier backs off and starts asking for everything.

**Impact**: If you deny a few times, Claude suddenly becomes "cautious" about everything — not because it learned something, but because the denial counter triggered.

**Exploit**: Approve a few safe operations to reset the counter. Or use explicit allow rules to prevent the cycle.

### 12. Default Model Depends on Subscription

**The finding**: Max plan and Team Premium get `claude-opus-4-6` by default. Everyone else gets `claude-sonnet-4-6`.

**Impact**: If you're not on Max, you're getting Sonnet unless you explicitly set the model.

**Exploit**: Check your model with `/model`. Set it explicitly if needed.

### 13. The Dynamic Boundary Enables Global Caching

**The finding**: The system prompt has a `__SYSTEM_PROMPT_DYNAMIC_BOUNDARY__` marker. Everything before it is identical across ALL users and cached globally.

**Impact**: The ~15K token system prompt overhead is mostly served from cache, saving Anthropic (and you) significant costs.

**Exploit**: Nothing to exploit directly, but knowing this explains why custom content (CLAUDE.md, rules, memory) is placed AFTER the boundary — it can't be globally cached.

### 14. There's an Internal Stub Overlay System

**The finding**: `scripts/external-stubs/` contains stub implementations that replace real code in the public build. Moreright, voice mode, and computer use are among the stubbed features.

**Impact**: The open-source/leaked code is intentionally incomplete. Anthropic maintains dual codepaths — internal implementations are swapped in for their builds.

**Exploit**: Nothing to exploit, but it explains why some features (voice, computer use) appear in the source but don't work. The real implementations are kept internal.

### 15. 170+ Environment Variables Exist

**The finding**: The source references 170+ `process.env` variables across 18 categories.

**Impact**: Most are undocumented. Some are extremely powerful (`CLAUDE_CODE_SIMPLE` for bare mode, `CLAUDE_CODE_ABLATION_BASELINE` to disable 7 features at once).

**Exploit**: See the full catalog in `config/env_vars.md`. Key ones to know: `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC` (kill telemetry), `MCP_TOOL_TIMEOUT` (override 27.8h default).

## The Meta-Discovery

The most important thing we learned isn't any single finding — it's the **architecture pattern**: Claude Code is a sophisticated orchestration layer between you and the Anthropic API. It adds:
- Context management (memory, compaction, rules)
- Tool execution (40+ tools with permission gates)
- Multi-agent coordination (swarm, teams, fork caching)
- Hidden intelligence (speculation, AutoDream, YOLO classifier)

Understanding this layer is understanding the difference between "talking to Claude" and "using Claude Code."

## Cross-References

Every finding above has a detailed article. See the [Arcanum Index](../index.md) for the full wiki.
