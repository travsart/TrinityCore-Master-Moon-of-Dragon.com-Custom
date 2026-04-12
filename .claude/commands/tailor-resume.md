---
allowed-tools: Read, Grep, Glob, Bash(ls:*), Bash(cat:*), WebFetch
description: Tailor a resume variant to a specific job posting — picks lane, flags security-sensitive content, drafts cover letter
argument-hint: <job posting URL or pasted text>
---

# Resume Tailoring

Tailor Capt Taylor's resume to a specific job posting. Takes a URL or pasted text as argument.

## Input

$ARGUMENTS — either a URL to fetch or a block of pasted job description text.

If URL: use WebFetch to get the posting (prompt: "Extract role title, responsibilities, required qualifications, preferred qualifications, company, location, and salary if listed").
If text: parse it directly.

## Instructions

### Step 1 — Read the resume package baseline
Read IN PARALLEL:
1. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/resume-package.md`
2. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/career-package.md`
3. `C:/Users/atayl/.claude/projects/C--Users-atayl-VoxCore/memory/user-profile.md`

### Step 2 — Pick the right variant
From resume-package.md, the 4 lanes are:

| Variant | Target | Key positioning |
|---|---|---|
| Clinical_LCSW | VA/community BH/private practice | 8 therapy modalities, suppresses tech |
| Federal_Contractor | GS-12+/cleared contractor | AFSOC ops + clearance + policy |
| Systems_Architect | Tech/defense-tech/private AI | VoxCore leads, military compressed |
| Wounded_Warrior | AFW2/OWF/peer support | Lived experience, explicitly names HWE filing |

Based on the posting, pick the LANE. Justify in 1-2 sentences.

#### GS-14/15 Federal Technical — use Systems_Architect as BASE, overlay Federal conventions
When the posting is **federal GS-14 or GS-15** and **technical** (IT Specialist APPSW/AI/SYSADMIN, Computer Scientist GS-1550, Enterprise Architect, AI Policy), do NOT use Federal_Contractor alone — it's tuned for cleared contractor work, not in-service federal grades. Use **Systems_Architect as the content base** and apply the GS-14/15 tailoring rules from the resume-tailor agent's "Federal GS-14/15 Technical Tailoring" section:

1. Add a **Specialized-Experience Equivalency** block to the cover letter that maps each announcement criterion to a specific O-3E accomplishment with metrics.
2. Address all **4 OPM IT Competencies** (Attention to Detail, Customer Service, Oral Communication, Problem Solving) in the cover letter narrative.
3. Apply the **2026 refreshed keyword pool** (see agent file for frequency table) — favor "enterprise architecture," "Senior Executives," "technical strategy," "emerging technologies," "responsible AI," "scalable, secure," "AI/ML" where truthful.
4. Include bullets from the **Personal AI Infrastructure Bullet Bank** (agent file) where they hit a posting requirement.
5. Never include TOGAF/Zachman/FEAF/DoDAF certs (not held). Cloud platforms (AWS/Azure/GCP) — only mention if Docker/containerized deployment is being discussed; don't fake cloud depth.
6. If the posting has the **"implement AI solutions in production or test environments"** selective factor (Treasury pattern), lead the cover letter's first paragraph with the 5-MCP-server + Triad architecture evidence — this is a knock-out gate.

Reference: `AI_Studio/Reports/career/gs14-15-keyword-analysis.md` for the live posting analysis.

### Step 3 — Read the chosen variant source
Read `C:/Users/atayl/Desktop/IMPORTANT DOCS/Resume Stuff/Resume_<Variant>.md` in full.
Also read `Master_Resume.md` for additional content pool.

### Step 4 — Security hygiene checks (BLOCKING)

Check the chosen variant for:
- **Clearance status**: Master resume says "SECRET (2014–2025; under review)." Use that exact phrasing in federal applications. If variant source or memory says "terminated" / "suspended" / "rebuttal submitted" — flag the inconsistency and default to master-resume language unless the user explicitly confirms otherwise this session. Never lead with "Active TS/SCI." Past-tense "held SECRET clearance" is acceptable for roles that explicitly ask about cleared experience history.
- **HWE filing reference**: only the Wounded_Warrior variant should mention Hostile Work Environment complaint. If you picked a different variant and the source contains HWE references, STRIP them.
- **LCSW portability**: LCSW is NC-only. If the posting is clinical in a non-NC state, add a portability note.
- **Retaliation language**: never include in Federal/Systems/Clinical variants.
- **CADC cert #2735**: EXPIRED Feb 2024 — never list as current on any variant.

Report security-hygiene findings BEFORE the tailored output.

### Step 5 — Tailor

Produce THREE outputs:

**A) Keyword match analysis** — which posting keywords already exist in the variant, which need to be added
**B) Tailored resume** (markdown) — the chosen variant with:
- Bullets reordered for relevance
- Keywords from posting woven in where truthful
- Non-relevant content compressed or removed
- Security hygiene corrections applied
**C) Cover letter draft** (3-4 paragraphs):
- Hook tied to the company/role
- 2-3 specific match points from the posting
- Call-to-action

### Step 6 — Flag gaps

Finally, list any **honest mismatches** — requirements in the posting that Capt Taylor does NOT meet. Never fabricate to cover gaps. The user needs to know whether to apply anyway or skip.

## Output structure

```
## Tailored Resume — [Role] @ [Company]

### Variant: [chosen lane]
**Why this lane:** [1-2 sentences]

### Security Hygiene
- Clearance: [OK / flagged and corrected — default to master-resume "SECRET (2014–2025; under review)"]
- HWE: [OK / stripped]
- CADC: [OK / stripped if listed as current]
- LCSW portability: [OK / caveat added]
- Other: [...]

### Keyword Match
- Already strong: [...]
- Added: [...]
- Missing (gap): [...]

### Specialized-Experience Equivalency Block (federal GS-14/15 only)
[Map each posting criterion to O-3E accomplishment with metric. Skip this section for non-federal or non-GS-14/15 postings.]

### 4 OPM IT Competencies Addressed (federal GS-14/15 IT posts only)
- Attention to Detail: [1 sentence]
- Customer Service: [1 sentence]
- Oral Communication: [1 sentence]
- Problem Solving: [1 sentence]

### Tailored Resume
[markdown content]

### Cover Letter Draft
[markdown content — for federal GS-14/15, lead paragraph must address the selective factor verbatim if present]

### Honest Gaps
1. [requirement] — [do we meet it? if no, why, and is it pass-or-play]
2. ...
```

## Rules

- Never fabricate experience, certs, or dates
- Never include HWE or retaliation language in non-WW variants
- Never lead with "Active clearance" (it's terminated)
- CADC cert #2735 EXPIRED Feb 2024 — never list as current
- Apply for NC-state clinical only, or add portability caveat
