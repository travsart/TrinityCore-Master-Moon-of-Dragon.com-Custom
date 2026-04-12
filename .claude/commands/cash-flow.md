---
allowed-tools: Read, Grep, Glob, Bash(ls:*), Bash(python3:*)
description: Finances quick-look — cash flow, deficit, highest-ROI pending actions, upcoming bills
---

# Cash Flow Dashboard

Compact finances snapshot with brutal-honesty framing (no hype).

## Instructions

Read IN PARALLEL:
1. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/finances-overview.md`
2. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/angel-va.md` — for TDIU leverage
3. `C:/Users/atayl/Desktop/IMPORTANT DOCS/Finances/INDEX.md` (if exists)
4. Latest file in `C:/Users/atayl/Desktop/IMPORTANT DOCS/Finances/01_Master_Plans/` (use `ls -t` to find newest)

## Output Format

```
## Cash Flow — [today]

### Monthly Baseline
| | Amount | Source |
|---|---:|---|
| Income (combined) | \$X,XXX | Military BAH+pay, Angel VA, SSDI? |
| Expenses | \$X,XXX | Mortgage, utilities, debts, food |
| **Deficit** | **\$X,XXX** | |

### Separation Cliff (post-10 Aug 2026)
- Income shift: lose military BAH+pay (\$X,XXX), gain VA severance/disability (\$X,XXX)
- Net change: +/- \$X,XXX
- Cozy Wellness LLC revenue (post-separation LCSW practice): UNPROVEN

### Highest-ROI Pending Actions
| Action | Monthly Impact | Status | Blocker |
|---|---:|---|---|
| File Angel TDIU 21-8940 | +\$1,738 | Drafted, not filed | Evidence gathering |
| File Angel migraine 50% supplemental | +~\$700 | Drafted, not filed | Headache diary, neuro notes |
| SSDI confirmation (filed 3/3/2026) | TBD | Unknown | Pull SSA confirmation |
| Identified expense cuts | +\$646 | Identified, not all executed | Execute the cuts |
| Cozy Wellness NM annual report | \$0 penalty | Due soon (biennial) | Check NM Secretary of State |

### Liability Surface
- Legal fees for case (06_Legal_Fees) — ongoing
- Housing: VA loan, ~\$232K balance at 6%, rental options all cash-flow negative
- CADC cert #2735 expired Feb 2024 — potential clinical income blocker

### Bottom Line
- Pre-action deficit: -\$1,370/mo
- Post-cuts deficit: -\$724/mo (still negative)
- TDIU filing alone would move Angel to +\$368/mo CUMULATIVE surplus (if cuts executed)
- Core strategy: TDIU + cuts + post-separation income. Bridging gap is the game.
```

## Rules

- **Brutal honesty on money** — per user directive, no hype, no speculative numbers
- Distinguish **proven** from **speculative** (Cozy revenue = speculative, Angel TDIU = proven if filed)
- If a number in memory is >30 days old and unverified, append "(stale, verify)"
- Flag the SSDI-confirmation pull as an open loop if it's been >30 days since filing
- Do NOT include account numbers, balances by account, or PII
