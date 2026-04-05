---
description: "git workflow commands — /diff /review /pr-comments /autofix-pr /issue /commit /commit-push-pr /security-review, GitHub integration, PR creation"
---

# Git Workflow Commands -- Arcanum Wiki

## Overview

These commands automate common git and GitHub workflows. Unlike most slash commands which are `local` or `local-jsx` type, several of these are `prompt` type -- they inject a carefully crafted prompt that instructs Claude to perform the git operation using tools. This means the model executes the git commands rather than the command running them directly.

## Commands

### /diff
- **Arguments**: None
- **What it does**: Views uncommitted changes and per-turn diffs. Shows what files have been modified during the current session and what the uncommitted git changes look like.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'diff',
  description: 'View uncommitted changes and per-turn diffs',
}
```

---

### /review
- **Arguments**: `[PR number]`
- **What it does**: Reviews a pull request. This is a `prompt` type command, meaning it injects instructions for Claude to perform the review. The behavior depends on feature gating:

  **Local review mode** (default): Injects a prompt that instructs Claude to:
  1. Run `gh pr list` if no PR number given
  2. Run `gh pr view <number>` for PR details
  3. Run `gh pr diff <number>` for the diff
  4. Analyze changes for correctness, conventions, performance, test coverage, and security

  **Ultrareview mode** (feature-gated): When `isUltrareviewEnabled()` is true, the command uses "Claude Code on the web" (CCR) for a remote, more thorough review process. This mode:
  - Checks remote agent eligibility
  - Teleports to a remote environment
  - Uses a dedicated ultraplan model
  - Has a 30-minute timeout
  - Requires accepting terms at `code.claude.com/docs/en/claude-code-on-the-web`
- **Feature gating**: Always available in local mode. Ultrareview requires the `tengu_review_bughunter_config.enabled` GrowthBook flag.
- **Key code**:
```typescript
const LOCAL_REVIEW_PROMPT = (args: string) => `
  You are an expert code reviewer. Follow these steps:
  1. If no PR number is provided, run \`gh pr list\`
  2. If a PR number is provided, run \`gh pr view <number>\`
  3. Run \`gh pr diff <number>\` to get the diff
  4. Analyze the changes...
  PR number: ${args}
`
```
- **Notes**: The ultrareview variant has legal considerations -- the description explicitly mentions "Claude Code on the web" and includes a terms URL per legal requirements.

---

### /pr-comments
- **Arguments**: `[PR number or additional context]`
- **What it does**: Fetches and displays comments from a GitHub pull request. This was originally a built-in prompt command but has been migrated to a plugin architecture via `createMovedToPluginCommand`. While the marketplace is private, it falls back to an inline prompt that instructs Claude to:
  1. Get PR info via `gh pr view --json`
  2. Fetch PR-level comments via GitHub API
  3. Fetch review comments via GitHub API
  4. Parse and format all comments with file/line context and diff hunks

  Output format includes @author, file path, line number, diff context, and quoted comment text.
- **Feature gating**: None stated, but uses the plugin migration pattern.
- **Key code**:
```typescript
export default createMovedToPluginCommand({
  name: 'pr-comments',
  description: 'Get comments from a GitHub pull request',
  progressMessage: 'fetching PR comments',
  pluginName: 'pr-comments',
  pluginCommand: 'pr-comments',
  async getPromptWhileMarketplaceIsPrivate(args) {
    return [{ type: 'text', text: `...prompt instructing gh api usage...` }]
  },
})
```
- **Notes**: Uses `jq` for JSON parsing in the prompt. When the marketplace goes public, the fallback prompt logic will be removed and this will be purely plugin-driven.

---

### /autofix-pr
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely an automated PR fix command.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /issue
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely a GitHub issue interaction command.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /commit
- **Arguments**: None (takes context from git state)
- **What it does**: A `prompt` type command that instructs Claude to create a git commit. The injected prompt:
  1. Gathers context: `git status`, `git diff HEAD`, current branch, recent commits
  2. Enforces a Git Safety Protocol (never amend, never skip hooks, never commit secrets)
  3. Analyzes staged changes and drafts a commit message
  4. Creates the commit with attribution text

  Has special handling for Anthropic "undercover" mode where employees work on external repos.
- **Allowed tools**: Only `Bash(git add:*)`, `Bash(git status:*)`, `Bash(git commit:*)`
- **Key code**:
```typescript
const ALLOWED_TOOLS = [
  'Bash(git add:*)',
  'Bash(git status:*)',
  'Bash(git commit:*)',
]
```
- **Notes**: This is the backend for the `/commit` slash command. The commit attribution text is configurable via `getAttributionTexts()`.

---

### /commit-push-pr
- **Arguments**: None (takes context from git state)
- **What it does**: An extended version of `/commit` that also pushes and creates a PR. The injected prompt:
  1. Performs all the commit steps from `/commit`
  2. Creates a new branch if on main/master
  3. Pushes the branch
  4. Creates a PR via `gh pr create` with enhanced attribution
  5. Optionally adds reviewers and changelog sections (for Anthropic employees)

  Has Slack integration awareness and can notify channels about the PR.
- **Allowed tools**: Broader than `/commit`:
  ```
  Bash(git checkout:*), Bash(git add:*), Bash(git status:*),
  Bash(git push:*), Bash(git commit:*), Bash(gh pr create:*),
  Bash(gh pr edit:*), Bash(gh pr view:*), Bash(gh pr merge:*),
  ToolSearch, mcp__slack__send_message
  ```
- **Notes**: Includes `getEnhancedPRAttribution()` which adds a more detailed attribution section to PR descriptions. Has special handling for Anthropic's internal reviewer (`anthropics/claude-code`).

---

### /security-review
- **Arguments**: None
- **What it does**: Performs a security-focused code review of the pending changes on the current branch. This is a `prompt` type command (migrated to plugin pattern) that:
  1. Gathers git context: status, modified files, commits, and full diff
  2. Reviews from a senior security engineer perspective
  3. Focuses on security-specific concerns (injection, auth, secrets, etc.)

  Uses frontmatter-based configuration with allowed tools limited to read-only git commands plus `Task` (for parallel analysis).
- **Allowed tools**: `Bash(git diff:*)`, `Bash(git status:*)`, `Bash(git log:*)`, `Bash(git show:*)`, `Bash(git remote show:*)`, `Read`, `Glob`, `Grep`, `LS`, `Task`
- **Key code**:
```typescript
const SECURITY_REVIEW_MARKDOWN = `---
allowed-tools: Bash(git diff:*), Bash(git status:*), Bash(git log:*), ...
description: Complete a security review of the pending changes
---
You are a senior security engineer conducting a focused security review...
`
```

## Hidden/Undocumented Commands

- **/autofix-pr** -- Stubbed out, permanently disabled.
- **/issue** -- Stubbed out, permanently disabled.
- **/commit** and **/commit-push-pr** -- These are standalone `.ts` files, not subdirectories. They are registered as commands but may not appear in the standard `/help` listing depending on how commands are loaded.
- **/security-review** -- Standalone `.ts` file, migrated to plugin pattern.
