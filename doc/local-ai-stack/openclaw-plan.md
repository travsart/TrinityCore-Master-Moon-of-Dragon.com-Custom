# OpenClaw + NemoClaw — Always-On Agent Plan

> Background AI agent for continuous case file monitoring and organization.

## What is OpenClaw

"An operating system for AI agents" — open source (250K+ GitHub stars, Apache 2.0).
Hub-and-spoke architecture: single Gateway → Agent Runtime → tools (browser, files,
scheduled jobs, Canvas). Supports Ollama as local LLM provider out of the box.

## What is NemoClaw

NVIDIA's open source layer on top of OpenClaw. Adds:
- Policy-based privacy and security guardrails
- Controls how agents handle sensitive data
- Everything stays offline and siloed
- Uses NVIDIA Agent Toolkit (OpenShell) for enforcement

## Why This Matters for the Case

1,579 case files. ADSCD: August 10, 2026. Need continuous organization without
burning API tokens. OpenClaw + Gemma 4 = always-on agent at zero marginal cost.

## Planned Agent Tasks

### 1. Auto-Indexer
- Watch `C:/Users/atayl/Desktop/Case_Reference/` for new files
- On new file: classify type, generate frontmatter, add to vector DB
- Update master index automatically

### 2. Contradiction Finder
- Nightly: sweep document pairs looking for inconsistencies
- "Command stated X in this MFR but stated Y in this email"
- Output contradiction report with citations to `AI_Studio/Reports/`

### 3. Cross-Reference Validator
- Check every claim in filings against source evidence
- Flag claims that lack supporting documents
- Flag documents that aren't cited in any filing

### 4. Missing Evidence Detector
- Compare filing requirements against available evidence
- "DD-7050 requires X, Y, Z — you have X and Y but Z is missing"
- Proactive alerts before submission deadlines

### 5. Timeline Keeper
- Extract dates from all documents
- Maintain master chronological timeline
- Flag gaps (periods with no documentation)

## Architecture

```
OpenClaw Gateway (background service)
    │
    ├── Ollama: gemma4:26b (default model for all tasks)
    │
    ├── ChromaDB (shared with Open WebUI)
    │     └── All case collections
    │
    ├── File watcher (Case_Reference folder)
    │
    ├── Scheduled jobs
    │     ├── Every hour: check for new files → auto-index
    │     ├── Nightly: contradiction sweep
    │     └── Weekly: evidence gap report
    │
    └── NemoClaw guardrails
          ├── No data leaves local machine
          ├── No external API calls for case data
          └── Audit log of all agent actions
```

## Setup Steps (when ready)

```bash
# Install OpenClaw
pip install openclaw

# Initialize
openclaw onboard

# Configure Ollama backend
# Edit ~/.openclaw/openclaw.json:
# "default_model": "ollama/gemma4:26b"

# Install NemoClaw guardrails
pip install nemoclaw
openclaw install nemoclaw

# Install skills
openclaw skills install agent-team-orchestration
```

## Caveats

- Tool calling with local models can be unreliable (community reports)
- Set `"reasoning": false` in model config to avoid formatting issues
- Start with single-agent mode (OpenClaw default) — multi-agent later
- Community recommends 14B+ models for stable function calling
- Gemma 4 26B has native function calling with structured JSON output

## Sources

- OpenClaw: https://github.com/openclaw
- NemoClaw: https://www.nvidia.com/en-us/ai/nemoclaw/
- OpenClaw + Gemma 4 guide: https://www.lushbinary.com/blog/openclaw-gemma-4-local-ai-agent-ollama-setup-guide-2026/
- OpenClaw architecture: https://ppaolo.substack.com/p/openclaw-system-architecture-overview
- Best models for OpenClaw: https://clawdbook.org/blog/openclaw-best-ollama-models-2026
