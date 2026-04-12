# Ollama Setup

> Installed April 5, 2026

## Installation

- **Method**: `winget install Ollama.Ollama`
- **Version**: v0.20.0
- **Binary**: `C:\Users\atayl\AppData\Local\Programs\Ollama\ollama.exe`
- **API endpoint**: `http://localhost:11434`
- **Service**: Runs as Windows service, auto-starts on boot
- **GPU**: RTX 5090, 32GB GDDR7, driver 595.97
- **CUDA**: Detected automatically by Ollama

## Model Inventory

| Model | Ollama Tag | Disk Size | VRAM Loaded | Context Headroom | Purpose |
|-------|-----------|-----------|-------------|-----------------|---------|
| Qwen 3.5 27B | `qwen3.5:27b-q4_K_M` | ~17GB | ~18GB | ~14GB | Legal reasoning, RAG |
| Gemma 4 26B MoE | `gemma4:26b` | ~17GB | ~15-18GB | ~14-17GB | Bulk processing, agent |
| nomic-embed-text | `nomic-embed-text:latest` | 274MB | ~300MB | N/A | Text embeddings |

### Planned Additions
| Model | Ollama Tag | Disk Size | Purpose |
|-------|-----------|-----------|---------|
| DeepSeek-R1 32B | `deepseek-r1:32b` | ~20GB | Chain-of-thought reasoning |
| Whisper | Via Python/faster-whisper | ~1.5GB | Audio transcription |

## Common Commands

```bash
# Full path (bash in Claude Code doesn't have it on PATH)
OLLAMA="/c/Users/atayl/AppData/Local/Programs/Ollama/ollama.exe"

# List models
$OLLAMA list

# Pull a model
$OLLAMA pull qwen3.5:27b-q4_K_M

# Run interactive chat
$OLLAMA run qwen3.5:27b-q4_K_M

# Check running models
$OLLAMA ps

# API: generate embedding
curl -s http://localhost:11434/api/embed \
  -d '{"model":"nomic-embed-text","input":"your text here"}'

# API: chat completion
curl -s http://localhost:11434/api/chat \
  -d '{"model":"qwen3.5:27b-q4_K_M","messages":[{"role":"user","content":"hello"}]}'

# API: list models
curl -s http://localhost:11434/api/tags
```

## Model Storage

Default: `C:\Users\atayl\.ollama\models\`
To change: set `OLLAMA_MODELS` environment variable.

## VRAM Management

Ollama loads/unloads models on demand. Only one large model in VRAM at a time.
The embedding model (274MB) can stay resident alongside any large model.
Swap time between models is ~2-5 seconds on NVMe.

## Troubleshooting

- **"command not found" in bash**: Use full path or add to PATH
- **Slow downloads**: Two concurrent pulls share bandwidth; pull sequentially for max speed
- **VRAM OOM**: Check `$OLLAMA ps` — may have two large models loaded. Unload with API call or wait for timeout (default 5 min idle)
- **nvidia-smi**: `nvidia-smi --query-gpu=name,memory.total,memory.free --format=csv,noheader`
