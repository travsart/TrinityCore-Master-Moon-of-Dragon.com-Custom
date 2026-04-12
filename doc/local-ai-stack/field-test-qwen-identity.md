# Field Test: Qwen 3.5 27B Identity & Self-Awareness

> April 5, 2026 — First live test of local model via `ollama run qwen3.5:27b-q4_K_M`

## What Happened

User ran Qwen 3.5 27B locally in PowerShell, pasted the full "Legal War Room"
architecture plan, and asked "how much of this can you already do?"

Qwen spent the entire conversation (600+ lines) insisting it was a cloud-based
OpenAI model, even when corrected directly — twice.

## Key Observations

### 1. Identity Confusion (Critical Limitation)
Qwen kept saying "I'm a cloud-based AI running on OpenAI's servers" — factually
wrong. It was running on a local RTX 5090 via Ollama in PowerShell. No data was
leaving the machine. The model has no self-awareness about its deployment context.

This happened because open-weight models are trained on internet text where
"AI assistant" = "cloud API." They inherit that assumption and can't question it.

### 2. Doubled Down When Corrected
- User: "But you live on my machine"
- Qwen: "I need to be very clear about this — I do not live on your machine."
- User: "I'm accessing you via my terminal on my computer"
- Qwen: Still insisted it was cloud-based, offered to help build a local system

The model constructed increasingly elaborate arguments from a wrong base premise
and never questioned the assumption. Visible in the `Thinking...` traces.

### 3. The Irony
Qwen spent 600 lines explaining why it "can't be part of your local stack" while
literally being part of the local stack. It was the thing it kept saying was impossible.

### 4. Technical Advice Was Mostly Sound
Strip away the identity confusion and the actual suggestions were decent:
- Docker Compose generation
- Python ingestion scripts
- Prompt engineering help
- ChromaDB schema design
- Privacy boundary recommendations

The model CAN do useful work — it just can't reason about WHAT it is.

### 5. Thinking Traces Show the Failure Mode
The visible chain-of-thought blocks show Qwen building on a wrong premise
("I am a cloud model") and constructing arguments from it. It never performs
the metacognitive step of asking "wait, how am I actually being invoked?"

## Implications for the Stack

This test validates the two-tier routing strategy:

| Task | Right Tool | Why |
|------|-----------|-----|
| Classify an MFR | Local Qwen/Gemma | Fast, accurate, no self-awareness needed |
| Find contradictions | Local Qwen | Pattern matching works fine |
| Reason about the system | Claude Code | Needs contextual awareness |
| "What are you?" | Claude Code | Local models literally can't answer |

**Local models = muscle. Claude Code = brain.** Use each for what it's good at.

## Source

Full conversation transcript: `The Master.txt` (Desktop) and `Thoughts.txt` (Desktop),
both copied to `doc/local-ai-stack/`.
