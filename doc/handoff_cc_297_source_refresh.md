# Handoff: Claude Code 2.1.97 Source Refresh + Report Update

**Created**: 2026-04-08
**From**: Main tab, session after 231
**To**: New Claude Code tab (dedicate this entire tab to the task — it's a sustained research + patching job)
**Estimated scope**: 1 extraction + ~22 report patches + memory file update + README update

---

## 0. Read these first (5 min)

1. `AI_Studio/0_Central_Brain.md` — Triad context
2. `doc/session_state.md` — claim your tab row
3. `memory/claude-code-internals.md` — the existing consolidated internals memory (currently reflects v2.1.88)
4. `AI_Studio/Reports/ClaudeCodeInternals/README.md` — report index with all 22 reports
5. This file

**Owning tab context**: You own EVERY file under `AI_Studio/Reports/ClaudeCodeInternals/` and `memory/claude-code-internals.md` until done. No other tab should touch them. Do NOT touch C++, SQL, or VoxCore game systems in this tab.

---

## 1. Why this exists

Our 22 Claude Code internals reports were written against source extracted from `@anthropic-ai/claude-code@2.1.88`. The CLI has since shipped through **2.1.97** (9 point releases). Several items in the reports are now factually wrong or incomplete — see Section 5 for the priority list.

The user wants a fresh extraction of 2.1.97 source and all 22 reports updated against it.

---

## 2. Extraction methodology (how v2.1.88 was done, repeat for 2.1.97)

The existing archive at `C:/Users/atayl/Desktop/claude-code-source/claude-code-source/` was recovered from the source map (`cli.js.map`) bundled with the npm package. The README in that directory confirms:

> "source code recovered from the source map (`cli.js.map`) bundled in the @anthropic-ai/claude-code@2.1.88 npm package"

### Steps to re-extract 2.1.97

```bash
# Set up a working directory on Desktop (NOT inside VoxCore repo)
mkdir -p "C:/Users/atayl/Desktop/claude-code-source/extract-2.1.97"
cd "C:/Users/atayl/Desktop/claude-code-source/extract-2.1.97"

# 1. Pull the exact tarball from npm
npm pack @anthropic-ai/claude-code@2.1.97
# Produces: anthropic-ai-claude-code-2.1.97.tgz

# 2. Extract the tarball
tar -xzf anthropic-ai-claude-code-2.1.97.tgz
# Produces: package/ directory containing cli.js, cli.js.map, etc.

# 3. Recover source from the sourcemap
# Use one of these tools — try in order:
#   a) sourcemap-to-source (if installed): sourcemap-to-source package/cli.js.map ./src
#   b) npx source-map-explorer package/cli.js (verifies map integrity)
#   c) Write a small Node script using the `source-map` npm package:
#
#      const {SourceMapConsumer} = require('source-map');
#      const fs = require('fs');
#      const path = require('path');
#      const raw = JSON.parse(fs.readFileSync('package/cli.js.map','utf8'));
#      SourceMapConsumer.with(raw, null, consumer => {
#        consumer.sources.forEach(s => {
#          const content = consumer.sourceContentFor(s, true);
#          if (!content) return;
#          const out = path.join('src', s.replace(/^webpack:\/\//, ''));
#          fs.mkdirSync(path.dirname(out), {recursive: true});
#          fs.writeFileSync(out, content);
#        });
#      });
#
# The v2.1.88 archive has ~1,902 TS files — expect similar for 2.1.97.

# 4. Sanity check: count TS files and compare to v2.1.88 baseline
find src -name '*.ts' -o -name '*.tsx' | wc -l
# Target: ≥1,900 files. If you see <1,000, the extraction missed sourcesContent.
```

### If npm is unavailable or the tarball is stripped

Fallback: use the installed binary on this machine. The user's running Claude Code version is **2.1.74** (upgrade pending), but the user may have already upgraded by the time you run. Check with `claude --version`. If it's 2.1.97, you can point at the local install:

```bash
# Typical global install locations on Windows
npm root -g
# Then: <globalRoot>/@anthropic-ai/claude-code/cli.js + cli.js.map
```

If you still can't get a tarball, **stop and ask the user** — do not proceed with a stale 2.1.88 source for a task whose whole purpose is refreshing against 2.1.97.

---

## 3. Final archive layout (target)

```
C:/Users/atayl/Desktop/claude-code-source/
  claude-code-source/                  -- v2.1.88 (keep for diff baseline — do NOT delete)
  extract-2.1.97/
    anthropic-ai-claude-code-2.1.97.tgz
    package/                           -- raw extracted tarball
    src/                               -- recovered TypeScript source (primary reference)
```

After extraction, **do not delete the old v2.1.88 tree**. You'll need it to diff directory-by-directory and identify what actually changed, so you don't re-read files that are byte-identical.

---

## 4. Diff strategy (don't re-read 1,900 files)

Don't burn context reading everything. Run a directory diff first:

```bash
# Fast structural diff — only lists changed files
diff -rq \
  "C:/Users/atayl/Desktop/claude-code-source/claude-code-source/src" \
  "C:/Users/atayl/Desktop/claude-code-source/extract-2.1.97/src" \
  > /tmp/cc_297_file_diff.txt

# Count changed files
wc -l /tmp/cc_297_file_diff.txt

# Changed files per subsystem
grep -E '(hooks|tools|mcp|permissions|skills|compaction|memdir|coordinator)' /tmp/cc_297_file_diff.txt
```

Read ONLY the changed files, grouped by the report subsystem they feed. Write the full changed-file list to `AI_Studio/Reports/ClaudeCodeInternals/_2.1.97_refresh/changed_files.txt` so you have a durable reference.

---

## 5. Priority update list (what the main tab already identified as wrong)

This list came from a changelog diff of 2.1.88 → 2.1.97. Use it as the **starting checklist** — the source diff may reveal more. Each item is tagged `[invalidates]` (report is factually wrong), `[gap]` (new surface area not covered), or `[refine]` (behavior nuance).

### Top 5 most-wrong reports (patch these first)

**`03_context_window.md` + `1m_context_deep_dive.md`**
- [invalidates] **1M context is default for Opus 4.6 on Max/Team/Enterprise** as of 2.1.75 — the `[1m]` suffix is no longer required on those plans. Current report says `[1m]` is always a client-side suffix that gets stripped and a beta header is injected. Needs a plan-tier default section.
- [invalidates] **Default max output tokens for Opus 4.6 = 64k** (2.1.77). Verify the exact constant in `src/constants/` or `src/services/api/`.
- [gap] Token counts ≥1M now display as `1.5m` format (2.1.84).
- [gap] Global system-prompt caching now works when ToolSearch is enabled (2.1.84).

**`09_hooks_system.md`** — SIGNIFICANT EXPANSION
Current memory lists 5 hook types. Current reality needs to enumerate all of these from `src/hooks/`:
- [gap] `PermissionDenied` hook (2.1.89)
- [gap] `TaskCreated` hook (2.1.84)
- [gap] `CwdChanged`, `FileChanged` hooks (2.1.83) — memory already has FileChanged
- [gap] `StopFailure` hook (2.1.78)
- [gap] `PostCompact` hook (2.1.76) — memory has it but verify report 09 does
- [gap] `Elicitation` and `ElicitationResult` hooks (2.1.76)
- [gap] `WorktreeCreate` hook now supports `type: "http"` (2.1.84)
- [gap] New `"defer"` decision value for `PreToolUse` hooks (2.1.89)
- [gap] Conditional `if` field on hooks using permission-rule syntax (2.1.85)
- [gap] `PreToolUse` hooks can satisfy `AskUserQuestion` by returning `updatedInput` (2.1.85)
- [invalidates] `Stop`/`SubagentStop` hooks were failing on long sessions until 2.1.97 — report should note the fix as behavior
- [gap] Hook `if` condition now matches compound commands and env-var prefixes (fixed 2.1.89)
- [gap] Hook output over 50K characters now saved to disk (2.1.89)

**`12_mcp_client.md`** — EXPAND
- [invalidates] **MCP HTTP/SSE connections were leaking ~50 MB/hr** until 2.1.97. Report 12 likely claims stability that wasn't there on any version the user ran. Document the leak, the fix, and the new buffer behavior.
- [gap] MCP elicitation protocol support + hooks (2.1.76).
- [gap] MCP tool descriptions capped at 2KB (2.1.84).
- [gap] MCP OAuth follows RFC 9728 Protected Resource Metadata discovery (2.1.85).
- [gap] MCP OAuth Client ID Metadata Document (CIMD / SEP-991) support (2.1.81).
- [gap] Result persistence override via `_meta["anthropic/maxResultSizeChars"]` (2.1.91).
- [gap] MCP servers configured locally + via claude.ai deduplicated (2.1.84).
- [gap] `deniedMcpServers` now blocks claude.ai MCP servers (fixed 2.1.85).
- [gap] `MCP_CONNECTION_NONBLOCKING=true` env var for `-p` mode (2.1.89).
- [gap] `headersHelper` scripts receive `CLAUDE_CODE_MCP_SERVER_NAME` + `CLAUDE_CODE_MCP_SERVER_URL` (2.1.85).
- [gap] MCP tool result persistence override via `_meta["anthropic/maxResultSizeChars"]` (2.1.91).
- [gap] Slack MCP tool calls get compact `Slacked #channel` header with clickable link (2.1.94).

**`06_tool_pipeline.md`**
- [invalidates] **`TaskOutput` tool deprecated (2.1.83)** — use `Read` on output file path. Current memory still lists `TaskOutputTool` in "Always-loaded" tools.
- [invalidates] **Agent tool `resume` parameter removed (2.1.77)**; `SendMessage` now auto-resumes stopped agents. If report 06 or 08 documents the old API signature, it's wrong.
- [gap] `Edit` tool now works on files viewed via `Bash` without separate `Read` call (2.1.89).
- [gap] `Read` tool uses compact line-number format and deduplicates unchanged re-reads (2.1.86).
- [gap] MCP tool descriptions capped at 2KB (2.1.84).
- [gap] New setting `disableSkillShellExecution` (2.1.91).
- [gap] PowerShell tool is now opt-in preview (2.1.84); permission-hardened in .90 and .97; respects `/env` (2.1.89).
- [refine] Write tool 60% faster on large files with tabs/`&`/`$` (2.1.92).
- [gap] Edit tool uses shorter `old_string` anchors reducing output tokens (2.1.91).

**`22_ui_renderer.md`** — NO_FLICKER mode is a whole new subsystem
- [gap] **`CLAUDE_CODE_NO_FLICKER=1` rendering mode** (2.1.89) is a new code path. Report 22 only covers the standard Ink renderer. Trace the NO_FLICKER path in source — it has ~15 bug fixes across 2.1.90–2.1.97 implying a nontrivial separate renderer.
- [gap] `Ctrl+O` focus view toggle in NO_FLICKER mode (2.1.97).
- [gap] Transcript search: press `/` in transcript mode `Ctrl+O` (2.1.83).
- [invalidates] **WASM yoga-layout was replaced** for scroll perf (2.1.85). Report (and memory) says "custom Yoga layout (src/native-ts/yoga-layout/)" — that path exists but is no longer scroll-critical.
- [gap] Vim mode moved into `/config` (2.1.92) — no standalone `/vim` command.
- [gap] Markdown blockquotes with continuous left bar (2.1.97).
- [gap] Context-low warning is now a transient footer notification (2.1.97).
- [refine] Line-by-line response streaming (2.1.78, disabled on Windows .81).

### Other reports with known updates

**`01_compaction_engine.md`**
- [gap] Autocompact has **circuit breaker: fails with actionable error after 3 consecutive failed attempts** (2.1.89).
- [invalidates] Compaction was writing **duplicate multi-MB subagent transcript files** until 2.1.97 — fix changes on-disk behavior.
- [refine] Thinking summaries no longer generate by default in interactive sessions (2.1.89).
- [gap] `PostCompact` hook (2.1.76) — cross-ref with report 09.

**`02_system_prompt_assembly.md`**
- [invalidates] **Nested CLAUDE.md files were re-injected dozens of times** in long sessions until 2.1.89. Report likely documents correct single-injection behavior — note the fix and verify message-assembly code in `src/context/` / `src/utils/messages/`.
- [refine] Tool schema bytes changing mid-session caused cache misses — fixed 2.1.89.

**`04_memory_pipeline.md`**
- [gap] `autoMemoryDirectory` setting for custom auto-memory storage directory (2.1.74 — probably in the 2.1.88 source already, verify).
- [refine] Memory filenames in "Saved N memories" notice highlight and open on click (2.1.86).

**`07_swarm_system.md`**
- [invalidates] **Subagents with worktree isolation were leaking CWD to parent** until 2.1.97. Report claims isolation completeness — needs update.
- [gap] Teammate panes not closing when leader exits — fixed 2.1.83.
- [gap] iTerm2 auto-mode detection for native split-pane teammates (2.1.83).
- [refine] `/stats` was undercounting tokens by excluding subagent usage until 2.1.89.
- [gap] Background subagents becoming invisible after context compaction — fixed 2.1.83.

**`08_coordinator.md`**
- [invalidates] Agent tool `resume` param removed; `SendMessage` auto-resumes (2.1.77).
- [gap] `TaskCreated` hook added (2.1.84) — new lifecycle event in coordinator.

**`10_permissions.md`**
- [gap] `managed-settings.d/` drop-in directory for independent policy fragments (2.1.83).
- [gap] `forceRemoteSettingsRefresh` policy setting (2.1.92).
- [gap] Auto-mode classifier denials fire `PermissionDenied` hook + appear in `/permissions` (2.1.89).
- [invalidates] Permission rules with JavaScript prototype property names were broken until 2.1.97 — test glob-match semantics against the fix.
- [invalidates] Managed-settings allow rules were persisting after admin removal until 2.1.97.
- [refine] `permissions.additionalDirectories` mid-session changes were broken until 2.1.97.
- [gap] `Edit(//path/**)` allow rules now check resolved symlink target (2.1.89).
- [gap] Accept Edits mode auto-approves filesystem commands with safe env var prefixes (2.1.97).

**`11_skills_system.md`**
- [gap] `effort` frontmatter support for skills and slash commands (2.1.80).
- [gap] Plugin skill hooks in YAML frontmatter no longer silently ignored (2.1.94).
- [gap] Plugin skills via `"skills": ["./"]` use frontmatter `name` for invocation (2.1.94).
- [gap] `keep-coding-instructions` frontmatter for plugin output styles (2.1.94).
- [gap] `disableSkillShellExecution` setting (2.1.91).
- [refine] Skill descriptions in `/skills` menu capped at 250 chars; sorted alphabetically (2.1.86).

**`17_bridge.md`** — BIG GAP
- [invalidates] `/remote-control` bridges CLI/VSCode sessions to claude.ai/code (2.1.79) — a whole new remote-session surface area.
- [gap] `allowedChannelPlugins` managed setting (2.1.84).
- [gap] Bridge session cards show local git repo info (2.1.97).
- [gap] Remote Control session titles AI-generated from first prompt (2.1.83, refined .86).
- [gap] Session quality survey (2.1.76, enterprise-configurable via `feedbackSurveyRate`).

**`18_commands_catalog.md`** — recount needed
- [gap] `/powerup` added (2.1.90) — interactive animated lessons
- [gap] `/tag` removed (2.1.92)
- [gap] `/vim` command removed (now in `/config`) (2.1.92)
- [refine] `/release-notes` is now an interactive version picker (2.1.92)
- [refine] `/cost` got per-model + cache-hit breakdown for subscription users (2.1.92)
- [refine] `/feedback` explains unavailability instead of disappearing (2.1.91)
- Recount total. Old baseline was ~90–99 depending on how stubs are counted.

**`19_api_layer.md`**
- [gap] `X-Claude-Code-Session-Id` header on API requests (2.1.86).
- [gap] `x-client-request-id` header (2.1.84).
- [gap] Line-by-line response streaming (2.1.78) — disabled on Windows (2.1.81).
- [gap] `CLAUDE_STREAM_IDLE_TIMEOUT_MS` env var (2.1.84).
- [gap] Non-streaming fallback cap raised 21k→64k, 2-minute per-attempt timeout (2.1.83).
- [gap] `ANTHROPIC_DEFAULT_{OPUS,SONNET,HAIKU}_MODEL_SUPPORTS` env vars (2.1.84).
- [gap] `ANTHROPIC_CUSTOM_MODEL_OPTION` env var for custom model picker entry (2.1.78).
- [invalidates] 429 retries were burning attempts on small `Retry-After` until 2.1.97.
- [invalidates] Agents appearing stuck after 429 with long `Retry-After` — fixed 2.1.94.
- [gap] Amazon Bedrock powered by Mantle: `CLAUDE_CODE_USE_MANTLE=1` (2.1.94).
- [gap] Default effort bumped from medium → high for API-key / Bedrock / Vertex / Foundry / Team / Enterprise (2.1.94).

**`20_messages_pipeline.md`**
- [invalidates] Nested CLAUDE.md re-injection (2.1.89) — cross-ref with report 02.
- [refine] Read tool deduplicates unchanged re-reads (2.1.86).
- [refine] Token overhead reduced when mentioning files with `@` (2.1.86).
- [refine] `[Image #N]` chip inserted at cursor on paste (2.1.83); no trailing space (2.1.89).
- [gap] Session transcript size reduced by skipping empty hook entries (2.1.97).
- [gap] Transcript accuracy with final token usage per block (2.1.97).

**`21_feature_flags.md`** — new env vars to catalog
- `CLAUDE_CODE_NO_FLICKER=1` (2.1.89)
- `CLAUDE_CODE_USE_MANTLE=1` (2.1.94)
- `CLAUDE_CODE_SUBPROCESS_ENV_SCRUB=1` (2.1.83)
- `CLAUDE_STREAM_IDLE_TIMEOUT_MS` (2.1.84)
- `MCP_CONNECTION_NONBLOCKING=true` (2.1.89)
- `CLAUDE_CODE_PLUGIN_KEEP_MARKETPLACE_ON_FAILURE` (2.1.90)
- `CLAUDE_CODE_PLUGIN_SEED_DIR` (multi-path) (2.1.79)
- `ANTHROPIC_CUSTOM_MODEL_OPTION` (2.1.78)
- `ANTHROPIC_DEFAULT_{OPUS,SONNET,HAIKU}_MODEL_SUPPORTS` (2.1.84)
- `FORCE_HYPERLINK` via settings.json (fixed 2.1.94)

**Reports probably unchanged** (but verify via diff):
- `05_autodream.md`
- `13_computer_use.md`
- `14_voice_mode.md`
- `15_buddy_system.md`
- `16_ultraplan.md`

If the diff reveals changes in these directories (`src/utils/dream/`, `src/utils/computerUse/`, `src/voice/`, `src/buddy/`, `src/utils/ultraplan/`), patch them. Otherwise add a header line: `> Verified unchanged vs v2.1.97 source.`

---

## 6. Glossary / codename updates

Add to `memory/claude-code-internals.md` codename table:
- **Mantle** — Amazon Bedrock auth path (2.1.94)
- **NO_FLICKER** — alternate terminal renderer mode (2.1.89+)
- **powerup** — interactive animated lessons feature (2.1.90, /powerup command)

Verify by grepping source for any new codenames in 2.1.97 diff.

---

## 7. Workflow

Parallelize aggressively — you own the whole tab, use it:

1. **Extract** (serial, ~5 min): download tarball, run sourcemap extraction, verify file count.
2. **Diff** (serial, ~2 min): `diff -rq` old vs new, write changed_files.txt.
3. **Fan out Explore agents** (parallel, ~3 waves):
   - Wave 1 (top-5 most-wrong): Explore agents against `src/tools/`, `src/hooks/`, `src/services/mcp/`, `src/constants/` (for context-window constants), `src/ink/` + NO_FLICKER paths.
   - Wave 2 (rest of Tier 2 + infra): `src/coordinator/`, `src/utils/swarm/`, `src/skills/`, `src/utils/permissions/`, `src/commands/`.
   - Wave 3 (Tier 3 verification + Tier 4): `src/bridge/`, `src/voice/`, `src/buddy/`, `src/utils/computerUse/`, `src/utils/messages/`, `src/services/api/`, `src/utils/feature-flags/` or wherever `feature()` lives.
4. **Patch reports** (parallel writes, one report per Edit call): each agent should return findings as a structured diff ("in report X, the claim Y on line Z is now wrong, new reality is W") so you can apply Edit calls directly without re-reading.
5. **Update `memory/claude-code-internals.md`** — consolidate the key changes into the summary bullets.
6. **Update `AI_Studio/Reports/ClaudeCodeInternals/README.md`** — bump the source version line, add a "Session XXX 2.1.97 refresh" section summarizing what changed, update the critical findings list with anything new/corrected.
7. **Commit**: one atomic commit, message like `docs(cc-internals): refresh all 22 reports against claude-code 2.1.97 source`

---

## 8. Rules & anti-patterns

**Do**:
- Write findings to disk incrementally — don't accumulate in context.
- Cite `src/path/file.ts:NNN` for every claim you add or correct.
- Quote the actual TypeScript — don't paraphrase.
- Flag any discrepancies between the changelog list (Section 5) and what you find in source — the changelog might be misleading.

**Don't**:
- Don't delete the v2.1.88 archive — keep it for diff.
- Don't touch any file outside `AI_Studio/Reports/ClaudeCodeInternals/`, `memory/claude-code-internals.md`, and `memory/MEMORY.md` (one-line update if version rolled).
- Don't rewrite reports from scratch — surgical edits only. These reports took real work to produce.
- Don't trust the changelog blindly. It sometimes omits internal refactors. Prefer source evidence.
- Don't read every one of the ~1,900 files. The diff tells you what changed.
- Don't skip the extraction verification step. If file count is <1,500 your sourcemap recovery is broken.

---

## 9. Success criteria (tab is done when)

- [ ] `C:/Users/atayl/Desktop/claude-code-source/extract-2.1.97/src/` exists with ≥1,900 TS files
- [ ] `changed_files.txt` exists and has been used to scope the updates
- [ ] All 22 numbered reports have been either patched or marked verified-unchanged
- [ ] `memory/claude-code-internals.md` version header updated from `v2.1.88` to `v2.1.97`
- [ ] `memory/claude-code-internals.md` codename table has Mantle, NO_FLICKER, powerup
- [ ] `AI_Studio/Reports/ClaudeCodeInternals/README.md` source version updated, refresh session documented
- [ ] Atomic commit landed
- [ ] `doc/session_state.md` row updated to COMPLETE with commit hash
- [ ] Send a one-line report to main tab via message or file: "2.1.97 refresh done, N reports patched, M files diffed, commit X"

---

## 10. Escalate to the user if

- npm refuses to serve `@anthropic-ai/claude-code@2.1.97` (yanked, rate-limited, auth required)
- Sourcemap extraction yields <1,500 files (extraction method broken, need to ask how user did 2.1.88)
- The diff reveals a subsystem that's been rewritten wholesale (`src/services/api/` rebuilt from scratch, etc.) — that's a scope decision
- Any report needs more than surgical edits to fix — the user should know before you burn time

---

## Quick-start paste for the new tab

```
Read doc/handoff_cc_297_source_refresh.md. That's the whole brief — all context, methodology, priority list, and success criteria are in it. Start with Section 0 (read these first) then Section 2 (extraction). Claim your row in doc/session_state.md before you touch anything. You own the whole ClaudeCodeInternals/ reports directory and memory/claude-code-internals.md until you're done.
```
