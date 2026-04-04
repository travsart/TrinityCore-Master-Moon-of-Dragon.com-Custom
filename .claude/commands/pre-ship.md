---
allowed-tools: Read, Write, Grep, Glob, Bash(grep:*), Bash(find:*), Bash(python3:*), Bash(git:*), Agent, mcp__mysql__*
description: Pre-ship audit for addons, tools, and apps — runs automated checks then spawns noob + bully + security reviewers in parallel
paths: tools/publishable/**
---

# Pre-Ship Audit

Run the full VoxCore pre-ship checklist against an addon, tool, or app before release.
Combines automated mechanical checks with multi-agent adversarial review.

## Arguments

$ARGUMENTS — Path to the project directory (e.g., `tools/publishable/VoxGM`) and optionally the project type: `addon`, `python`, `cpp`, or `hybrid`. If type is omitted, detect from file extensions.

## Phase 1: Identify the Project

1. Parse $ARGUMENTS to get the project path and type
2. If no path given, ask the user
3. List all files in the project directory (recursive)
4. Detect project type from extensions: `.lua`/`.toc` = addon, `.py` = python, `.cpp`/`.h` = cpp, mixed = hybrid
5. Identify the project name from the directory name or TOC title
6. Read the primary README.md (if exists)

## Phase 2: Automated Mechanical Checks

Optionally, launch a `grep-auditor` agent and a `doc-auditor` agent in parallel to assist with these checks.
If those agent types aren't available, perform the checks directly.

Run ALL of the following checks. Report each as PASS/FAIL with evidence.

### 2A: Naming Consistency
- Grep the entire project for common old-name patterns. If the project was renamed, search for: the directory name minus common suffixes, any names mentioned in git log for renamed files
- Verify the TOC `## Title` matches the directory name
- If C++ files exist, verify `AddSC_` function names match file names
- Check that SavedVariables names in TOC match the Lua global table name

### 2B: Non-ASCII Scan
- Run: `grep -rPn '[\x80-\xFF]' <project_path>/ --include='*.lua' --include='*.py' --include='*.cpp' --include='*.h' --include='*.md' --include='*.toc'`
- Report any hits with file:line and the offending character's hex value
- Em dashes (U+2014), smart quotes, BOM markers are the usual culprits

### 2C: TOC Integrity (addon projects only)
- Parse the `.toc` file and extract all listed Lua files
- Glob for all `.lua` files in the project
- Diff: files in TOC but not on disk = FAIL. Files on disk but not in TOC = WARNING
- Verify `## Interface`, `## Title`, `## Notes`, `## Version`, `## SavedVariables` are all present

### 2D: Version Consistency
- Extract version string from: TOC `## Version`, README badge/header, any Lua `VERSION` variable, any `version` in Python setup
- Compare all extracted versions. Any mismatch = FAIL

### 2E: Documentation Path Verification
- Read every `.md` file in the project
- Extract all file paths mentioned in docs (look for patterns like `path/to/file`, backtick-quoted paths, code blocks with file references)
- For each extracted path, check if it exists relative to the project root
- Report any referenced-but-missing files as FAIL

### 2F: Cross-Doc Consistency
- If multiple docs exist (README, SETUP, HOOKS, etc.), extract all instructions that reference file names or commands
- Flag any contradictions between documents

### 2G: Secret Scan
- Grep for: `password`, `token`, `secret`, `api_key`, `apikey`, `localhost`, `127.0.0.1`, `C:\\Users`, `/home/`, `C:/Users`
- Exclude false positives in comments about "set your token" etc.
- Any actual credential or absolute path = FAIL

### 2H: Dead Code Detection (addon projects)
- For each Lua file, extract all top-level function declarations
- Check which functions are actually called from other files or registered as callbacks
- Report declared-but-never-called functions as WARNING

### 2I: Distribution Completeness
- If a `LICENSE` file exists: PASS. Otherwise: FAIL
- Check for `.gitignore` in project root
- Check that no `.pyc`, `__pycache__`, `.env`, `.vscode`, `*.log` files are present

## Phase 3: Multi-Agent Adversarial Review

After Phase 2 completes, launch THREE agents in PARALLEL:

### Agent 1: Noob Reviewer
Spawn an app-reviewer agent (or general-purpose if app-reviewer type is not available) with this prompt:
```
You are a WoW player who just found this addon/tool. You have never used TrinityCore.
You've played retail WoW for 2 years. You know how to install addons from CurseForge
but have never compiled anything or used command-line tools.

Your job: Walk through the README and setup instructions step by step.
At each step, answer:
- Do I understand what this is asking me to do?
- Is there jargon I don't know? (What is "Eluna"? What is "RBAC"? What is a "hook"?)
- If something goes wrong, will I know? Does it tell me?
- Would I give up at this step?

Read all .md files in [PROJECT_PATH]. Then give a brutal assessment:
- "I would give up at step N because..."
- "I don't understand what X means"
- "There's no way to tell if this worked"
- Rate 1-10: How likely am I to successfully use this?

Be specific. Quote the exact text that confused you.
```

### Agent 2: TC Discord Bully
Spawn an app-reviewer agent (or general-purpose if app-reviewer type is not available) with this prompt:
```
You are a senior TrinityCore developer on the TC Discord who has seen hundreds of
poorly-made custom scripts. You are technically excellent but socially abrasive.
You judge code by TC upstream standards and will roast anything that looks amateur.

Your job: Review every source file in [PROJECT_PATH].
Look for:
- Naming convention violations (TC uses PascalCase for classes, camelCase for locals)
- Unnecessary complexity or over-engineering
- Copy-pasted code that should be a function
- Framework misuse (wrong WoW API calls, deprecated patterns)
- "This looks like ChatGPT wrote it" tells (unnecessary comments, overly verbose patterns,
  functions that do nothing, abstractions with one implementation)
- Security issues (command injection, unsanitized input)
- Missing error handling where it actually matters
- README claims that the code doesn't support
- Any file that shouldn't be in a public release

Also check:
- Is the zip structure correct? Does it have a parent folder?
- Are there leftover files from previous names/iterations?
- Would you trust this code on your server?

Write your review as a Discord message. Be direct. Use TC community tone:
"Why does this exist?" "This is a footgun." "Did you test this?"
Rate: Would this survive 5 minutes on the TC Discord custom-scripts channel?
```

### Agent 3: Security & Stability Auditor
Spawn an app-reviewer agent (or general-purpose if app-reviewer type is not available) with this prompt:
```
You are a server administrator who has been burned by bad addons crashing servers
and bad scripts corrupting databases. You trust nothing.

Your job: Review every source file in [PROJECT_PATH].
Look for:
- SQL injection vectors (string concatenation in queries)
- Command injection (unsanitized user input passed to GM commands)
- Unbounded memory growth (tables that grow without limits, missing ring buffer caps)
- Missing nil guards that could cause Lua errors under load
- Race conditions (deferred callbacks accessing stale state)
- Database operations without transactions or rollback paths
- Destructive operations without confirmation or dry-run
- Crash vectors (division by zero, infinite loops, stack overflow from recursion)
- Performance concerns (O(n^2) loops, allocations in OnUpdate/CLEU)
- Data loss scenarios (what happens on disconnect mid-operation?)

For each finding, rate: CRITICAL (will crash/corrupt), HIGH (will break under load),
MEDIUM (will annoy users), LOW (cosmetic/style).

Give a trust verdict: "Would you install this on a production server?"
```

## Phase 4: Synthesis

After all three agents return:

1. Combine findings into a single report organized by severity
2. Cross-reference: if Agent 2 (bully) and Agent 3 (security) flag the same code, it's HIGH priority
3. Separate findings into:
   - **BLOCKING** — Must fix before shipping (crashes, security, missing files, broken docs)
   - **SHOULD FIX** — Professional quality issues (dead code, naming, missing tooltips)
   - **NICE TO HAVE** — Polish items (extra docs, better error messages)
4. Check findings against the comprehensive checklist in `addon-building-checklist.md` (memory file) — are there checklist items that none of the agents caught?

## Output Format

Write the audit report as a structured document. Fill in ALL sections with actual findings — no placeholder text.

```markdown
## Pre-Ship Audit: {project_name} v{version}

### Automated Checks
| Check | Status | Details |
|-------|--------|---------|
| Naming | {PASS or FAIL} | {specific findings or "No old names found"} |
| Non-ASCII | {PASS or FAIL} | {count and locations, or "Clean"} |
| TOC Integrity | {PASS, FAIL, or SKIP} | {missing/unlisted files, or "All files match"} |
| Version Consistency | {PASS or FAIL} | {extracted versions and mismatches} |
| Doc Paths | {PASS or FAIL} | {broken references, or "All paths valid"} |
| Cross-Doc | {PASS, FAIL, or SKIP} | {contradictions found, or "Consistent"} |
| Secrets | {PASS or FAIL} | {findings with file:line, or "Clean"} |
| Dead Code | {PASS or WARN} | {unreferenced functions, or "All functions called"} |
| Distribution | {PASS or FAIL} | {missing LICENSE, banned files, etc.} |

### Noob Review (Agent 1)
{Agent's full summary — survivability rating 1-10, specific confusion points, "would give up at" steps}

### TC Bully Review (Agent 2)
{Agent's full review — Discord survival rating, specific roasts, code quality concerns}

### Security Audit (Agent 3)
{Agent's full assessment — trust verdict, specific vulnerability findings by severity}

### Combined Findings
#### BLOCKING (must fix)
{Numbered list of blocking findings with file:line references. If none: "None — gate PASS"}

#### SHOULD FIX (professional quality)
{Numbered list of quality findings}

#### NICE TO HAVE (polish)
{Numbered list of polish items}

### Checklist Coverage
{List specific checklist items from addon-building-checklist.md that no agent or automated check covered. If all covered: "Full coverage achieved"}
```

## Phase 5: Write Gate Status

After synthesis, write the machine-readable gate status file to `.claude/release-gate-status.json`.

This file is created fresh by each `/pre-ship` run — it does not exist until the first audit.
It is read by enforcement hooks:
- `PreToolUse` hook blocks `git push --tags`, `gh release create`, zip creation when status != "PASS"
- `PostToolUse` hook invalidates status to "STALE" when publishable files are edited after a PASS

Write this JSON structure using the Write tool:

```json
{
    "status": "PASS or FAIL",
    "project": "project_name",
    "project_path": "tools/publishable/ProjectName",
    "version": "detected_version",
    "timestamp": "ISO 8601 UTC",
    "blockers": ["finding title 1", "finding title 2"],
    "should_fix_count": 0,
    "nice_to_have_count": 0,
    "agents_run": ["noob", "bully", "security"],
    "automated_checks": {
        "naming": "PASS",
        "non_ascii": "PASS",
        "toc_integrity": "PASS",
        "version_consistency": "PASS",
        "doc_paths": "PASS",
        "cross_doc": "SKIP",
        "secrets": "PASS",
        "dead_code": "WARN",
        "distribution": "PASS"
    }
}
```

Set `status` to `"PASS"` only when `blockers` is empty. Otherwise set `"FAIL"`.

## Rules

- Run ALL Phase 2 checks before launching Phase 3 agents — agents need the context
- Launch all three Phase 3 agents in PARALLEL — never sequential
- Do not suppress agent findings. Even if you disagree, report them
- If the project has multiple components (server + client), run checks on BOTH
- ALWAYS write release-gate-status.json at the end — this drives enforcement hooks
- After the audit, ask: "Want me to fix the BLOCKING items?"
