---
allowed-tools: Read, Write, Edit, Grep, Glob, Bash(grep:*), Bash(find:*), Bash(python3:*), Bash(sed:*), Agent
description: Fix only the BLOCKING items from the last /pre-ship audit — surgical remediation, not a full re-audit
paths: tools/publishable/**, .claude/release-gate-status.json
---

# Release Gate Fix

Fix the open BLOCKING items from the most recent `/pre-ship` audit.
This is the remediation companion to `/pre-ship` — it fixes, not re-audits.

## Arguments

$ARGUMENTS — Optional: specific blocker to fix (e.g., "em dashes", "dead code", "rename").
If empty, fix ALL open blockers from the gate status file.

## Process

### Step 1: Load Gate State

Read `.claude/release-gate-status.json`. If it doesn't exist or status is "UNKNOWN", tell the user
to run `/pre-ship` first — you need findings to fix.

Extract the `blockers` array. If $ARGUMENTS specifies a particular blocker, filter to just that one.

### Step 2: Fix Each Blocker

For each blocker, apply the minimal fix:

**"Non-ASCII characters":**
- Run `grep -rPn '[\x80-\xFF]' <project_path>/ --include='*.lua' --include='*.py' --include='*.cpp' --include='*.h'`
- Replace em dashes (U+2014) with `--`
- Replace smart quotes with straight quotes
- Remove BOM markers

**"Old/renamed identifiers":**
- Grep for the old name patterns identified in the audit
- Replace with the canonical new name using Edit with `replace_all: true`
- Check AddSC_ functions, RBAC constants, chat prefixes, command names

**"Dead code":**
- Read each flagged file
- Remove functions/files that are never called
- Remove from TOC if applicable
- Update README to remove claims about removed features

**"Version mismatch":**
- Identify the authoritative version (usually TOC `## Version`)
- Update all other locations to match

**"Missing documentation":**
- Add missing sections (glossary, troubleshooting, "Who is this for?")
- Link existing screenshots
- Fix path references

**"Dev artifacts in distribution":**
- Delete files that shouldn't ship (audit request docs, .git subdirs, reference/ clones)
- Update .gitignore if needed

**"False compatibility claims":**
- Add a "VoxCore-Specific Features" section to README
- Mark custom commands clearly
- Update the compatibility statement to be honest

**"Wrong labels":**
- Fix the label text to match the actual command behavior
- Verify by reading the command being sent

### Step 3: Targeted Re-Verification

After fixing each blocker, run ONLY the relevant automated check from `/pre-ship` Phase 2
to verify the fix landed. Don't re-run the full audit — just the targeted check.

### Step 4: Update Gate Status

Read the current gate status file. Remove fixed blockers from the `blockers` array.
If no blockers remain, set status to "PASS". Otherwise keep "FAIL" with remaining blockers.
Write the updated file.

## Rules

- Fix ONE blocker at a time. Verify before moving to the next.
- Don't introduce new issues while fixing old ones (re-verify after each fix).
- If a fix requires architectural changes, STOP and ask — this skill is for surgical fixes only.
- Don't fix SHOULD FIX or NICE TO HAVE items — those are separate work.
- After all blockers are fixed, suggest: "Gate is PASS. Ready to run /pre-ship for final confirmation?"
