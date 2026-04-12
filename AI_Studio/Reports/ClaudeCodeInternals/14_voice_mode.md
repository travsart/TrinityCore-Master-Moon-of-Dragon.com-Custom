# Voice Mode (Push-to-Talk) — Claude Code Internals Report

> **Report**: #14 of the Claude Code Internals series
> **Date**: 2026-04-05 (original); 2.1.97 refresh 2026-04-08
> **Source**: v2.1.88 `src/` baseline + cli.js@2.1.97 grep verification
> **Scope**: Complete voice/push-to-talk subsystem — audio capture, STT, UI, keybindings, feature gating
>
> **2.1.97 delta** (grep spot-check): 2 substantive changes since v2.1.88:
> 1. **Deepgram Nova 3 is now unconditional.** The `tengu_cobalt_frost` GrowthBook gate was removed. `use_conversation_engine=true` and `stt_provider=deepgram-nova3` are now hard-coded in the WebSocket query-params builder (cli.js@2.1.97 line 3748). The `[voice_stream] Nova 3 gate enabled` debug log is gone.
> 2. **The VoiceModeNotice startup banner was replaced by a spinner tip.** "Use /voice to enable push-to-talk dictation" now appears as a rotating tip (`id:"voice-mode"`, `cooldownSessions:10`, cli.js@2.1.97 line ~8339) instead of a dedicated startup notice. The eligibility predicate (voice available, not yet enabled, not remote/SSH) is preserved.
>
> All other voice mechanics verified unchanged: WebSocket flow (`/api/ws/speech_to_text/voice_stream`), `VOICE_STREAM_BASE_URL`, hold-to-talk keyhandler, audio capture NAPI path (`AUDIO_CAPTURE_NODE_PATH`), ALSA fallback (`arecord`/`S16_LE`), all tengu analytics events, and the `voiceNoticeSeenCount` / `voiceFooterHintSeenCount` config counters.

---

## Overview

Claude Code includes a fully-featured push-to-talk voice input system that captures audio from the user's microphone, streams it over a WebSocket to Anthropic's `voice_stream` speech-to-text endpoint (backed by Deepgram), and injects the transcribed text into the prompt input field. The system is designed around a hold-to-talk interaction model: the user holds a configurable key (default: Space), audio streams in real-time to the STT backend, and upon release the accumulated transcript replaces the cursor position in the input.

The voice subsystem is gated behind the `VOICE_MODE` compile-time feature flag (Bun's `bun:bundle` `feature()`) AND a runtime GrowthBook kill-switch (`tengu_amber_quartz_disabled`). It requires Anthropic OAuth authentication (Claude.ai account) — API keys, Bedrock, Vertex, and Foundry are all excluded. The system is Anthropic-internal ("ant builds" only); external/OSS builds dead-code-eliminate the entire voice subsystem at compile time.

Audio capture uses a native NAPI module (`audio-capture-napi`) built on the `cpal` Rust crate, supporting macOS (CoreAudio), Windows, and Linux (ALSA). On Linux, fallback paths exist for `arecord` (ALSA utils) and SoX (`rec`). All audio is captured as 16kHz, 16-bit signed, mono PCM — the same format expected by the WebSocket endpoint.

## Architecture

### File Map

| File | Purpose |
|------|---------|
| `src/voice/voiceModeEnabled.ts` | Feature gating: compile-time + runtime checks (GrowthBook, OAuth) |
| `src/services/voice.ts` | Audio recording layer: native NAPI, arecord, SoX backends |
| `src/services/voiceStreamSTT.ts` | WebSocket client for Anthropic's `voice_stream` STT endpoint |
| `src/services/voiceKeyterms.ts` | Domain-specific vocabulary hints for Deepgram STT accuracy |
| `src/hooks/useVoice.ts` | Core React hook: orchestrates recording, streaming, transcript assembly |
| `src/hooks/useVoiceEnabled.ts` | React hook: combines settings + auth + GrowthBook for render path |
| `src/hooks/useVoiceIntegration.tsx` | Keybinding handler, input integration, interim transcript display |
| `src/context/voice.tsx` | React context/store for voice state (recording/processing/idle, audio levels) |
| `src/components/PromptInput/VoiceIndicator.tsx` | UI: "listening..." and "Voice: processing..." shimmer |
| `src/components/PromptInput/VoiceIndicator.tsx` (VoiceWarmupHint) | UI: "keep holding..." during warmup |
| `src/components/LogoV2/VoiceModeNotice.tsx` | UI: "Voice mode is now available" startup banner |
| `src/components/PromptInput/Notifications.tsx` | Replaces all notifications with VoiceIndicator during recording |
| `src/components/TextInput.tsx` | Cursor becomes a waveform visualizer during recording |
| `src/components/PromptInput/PromptInputFooterLeftSide.tsx` | Footer hint: "hold Space to speak" |
| `src/commands/voice/index.ts` | `/voice` command registration (availability, visibility) |
| `src/commands/voice/voice.ts` | `/voice` command handler: toggle, pre-flight checks, mic permission |
| `src/keybindings/defaultBindings.ts` | Default binding: `space` -> `voice:pushToTalk` (Chat context) |
| `src/screens/REPL.tsx` | Wires `useVoiceIntegration` + `VoiceKeybindingHandler` into main REPL |

### Data Flow

```
User holds Space
    |
    v
useVoiceKeybindingHandler (detects rapid key repeat > HOLD_THRESHOLD)
    |
    v
useVoice.handleKeyEvent() -> startRecordingSession()
    |
    +----> voice.ts: startRecording() -> native NAPI / arecord / SoX
    |         |
    |         +----> onData(chunk: Buffer) -> audioBuffer[] (while WS connecting)
    |
    +----> voiceStreamSTT.ts: connectVoiceStream() -> WebSocket to voice_stream
    |         |
    |         +----> onReady(conn) -> flush audioBuffer, direct-send subsequent chunks
    |         |
    |         +----> onTranscript(text, isFinal)
    |                   |
    |                   +----> Interim: update voiceInterimTranscript in context store
    |                   +----> Final: accumulate in accumulatedRef
    |
User releases Space (no key repeat detected within 200ms)
    |
    v
finishRecording()
    |
    +----> voice.ts: stopRecording()
    +----> conn.finalize() -> sends CloseStream, waits for TranscriptEndpoint
    +----> Injects accumulated transcript into prompt input via onTranscript callback
    +----> State: recording -> processing -> idle
```

## Key Implementation Details

### 1. Hold-to-Talk Detection (no keyUp in terminals)

Terminals do not provide `keyUp` events. The system simulates key release detection using auto-repeat timing. When a key is held, the terminal emits repeated keypress events at 30-80ms intervals. The system uses a 200ms gap detector:

```typescript
// src/hooks/useVoice.ts:160
const RELEASE_TIMEOUT_MS = 200

// src/hooks/useVoice.ts:1094-1124
} else if (currentState === 'recording') {
  seenRepeatRef.current = true
  // ... clear and re-arm release timer
}
if (stateRef.current === 'recording' && seenRepeatRef.current) {
  releaseTimerRef.current = setTimeout(
    // ... finishRecording if no keypress within 200ms
    RELEASE_TIMEOUT_MS,
  )
}
```

### 2. Bare-Char vs Modifier-Combo Activation

Two binding types have different activation mechanics (`src/hooks/useVoiceIntegration.tsx:349-371`):

- **Modifier combos** (e.g., `meta+k`, `ctrl+x`): Activate on first press — unambiguous intent, no hold threshold needed. The letter part auto-repeats while held.
- **Bare chars** (e.g., Space, `v`): Require `HOLD_THRESHOLD = 5` rapid presses (120ms gap) to activate. The first `WARMUP_THRESHOLD = 2` presses flow through to the input (so a single space types normally). On activation, flow-through chars are stripped.

```typescript
// src/hooks/useVoiceIntegration.tsx:39
const RAPID_KEY_GAP_MS = 120;
// src/hooks/useVoiceIntegration.tsx:51
const HOLD_THRESHOLD = 5;
// src/hooks/useVoiceIntegration.tsx:54
const WARMUP_THRESHOLD = 2;
```

### 3. Audio Capture Pipeline

**Format**: 16kHz sample rate, 16-bit signed PCM, mono (1 channel).

**Backend priority** (`src/services/voice.ts:335-396`):
1. **Native NAPI** (`audio-capture-napi` via `cpal` Rust crate) — macOS, Linux (with ALSA cards), Windows
2. **arecord** (ALSA utils) — Linux fallback, probed with a 150ms device-open test
3. **SoX** (`rec` command) — Linux/macOS fallback, with `--buffer 1024` to prevent buffering delays

The native module is lazy-loaded on first voice keypress, not at startup. On macOS, this triggers the CoreAudio `dlopen` which blocks for 1-8 seconds on cold starts.

```typescript
// src/services/voice.ts:16-19
// audio-capture.node links against CoreAudio.framework + AudioUnit.framework;
// dlopen is synchronous and blocks the event loop for ~1s warm, up to ~8s on
// cold coreaudiod (post-wake, post-boot). Load happens on first voice keypress
```

Silence detection is **disabled** for push-to-talk (`silenceDetection: false`). The native module's internal silence callback is ignored; recording runs until the user releases the key.

### 4. WebSocket STT Protocol

**Endpoint**: `/api/ws/speech_to_text/voice_stream` on `api.anthropic.com` (not `claude.ai`, to avoid Cloudflare TLS fingerprint challenges).

**Wire format** (`src/services/voiceStreamSTT.ts`):
- **Client -> Server**: Binary audio frames (raw PCM), JSON control messages (`KeepAlive`, `CloseStream`)
- **Server -> Client**: JSON messages (`TranscriptText`, `TranscriptEndpoint`, `TranscriptError`)

**Connection parameters** (query string):
```
encoding=linear16
sample_rate=16000
channels=1
endpointing_ms=300
utterance_end_ms=1000
language=en (or user-configured BCP-47 code)
keyterms=MCP,symlink,grep,... (up to 50 domain-specific terms)
```

**STT Provider**: Deepgram Nova 3 (when `tengu_cobalt_frost` GrowthBook flag is enabled), routed through Anthropic's conversation-engine. Parameters include `use_conversation_engine=true` and `stt_provider=deepgram-nova3`.

**Keepalive**: Initial `KeepAlive` sent immediately on open, then every 8 seconds.

**Finalize flow**: When recording stops, `CloseStream` is sent (deferred to next event-loop tick to flush queued audio). Resolution triggers:
1. `TranscriptEndpoint` after `CloseStream` (~300ms — fast path)
2. No-data timeout (1.5s — server has nothing)
3. WebSocket close event (~3-5s — server teardown)
4. Safety timeout (5s — last resort)

### 5. Silent-Drop Replay

Approximately 1% of sessions hit a "session-sticky CE pod" bug where the server accepts audio but returns zero transcripts. When `finalize()` resolves via `no_data_timeout` with `hadAudioSignal=true`, the system replays the buffered audio on a fresh WebSocket connection once:

```typescript
// src/hooks/useVoice.ts:241-246
// Full audio captured this session, kept for silent-drop replay. ~1% of
// sessions get a sticky-broken CE pod that accepts audio but returns zero
// transcripts; when finalize() resolves via no_data_timeout with
// hadAudioSignal=true, we replay the buffer on a fresh WS once.
// Bounded: 32KB/s x ~60s max = 2MB.
const fullAudioRef = useRef<Buffer[]>([])
```

### 6. Audio Level Visualization

RMS amplitude is computed from 16-bit PCM buffers using a sqrt curve for better visual range:

```typescript
// src/hooks/useVoice.ts:185-197
export function computeLevel(chunk: Buffer): number {
  const samples = chunk.length >> 1
  if (samples === 0) return 0
  let sumSq = 0
  for (let i = 0; i < chunk.length - 1; i += 2) {
    const sample = ((chunk[i]! | (chunk[i + 1]! << 8)) << 16) >> 16
    sumSq += sample * sample
  }
  const rms = Math.sqrt(sumSq / samples)
  const normalized = Math.min(rms / 2000, 1)
  return Math.sqrt(normalized)
}
```

A 16-bar histogram is maintained in `audioLevelsRef` and published to the voice context store. The cursor in `TextInput.tsx` renders as a mini waveform during recording using Unicode block characters, with 50ms animation frame updates.

### 7. Voice Keyterms for STT Accuracy

`src/services/voiceKeyterms.ts` builds a vocabulary hint list (max 50 terms) sent as query parameters to boost Deepgram recognition:

**Global terms**: `MCP`, `symlink`, `grep`, `regex`, `localhost`, `codebase`, `TypeScript`, `JSON`, `OAuth`, `webhook`, `gRPC`, `dotfiles`, `subagent`, `worktree`

**Dynamic terms**: Project root basename, git branch words (split on camelCase/kebab/snake), recent file name words.

### 8. Language Support

19 languages supported (`src/hooks/useVoice.ts:42-114`):

en, es, fr, ja, de, pt, it, ko, hi, id, ru, pl, tr, nl, uk, el, cs, da, sv, no

Language resolution (`normalizeLanguageForSTT`): checks `settings.language` against BCP-47 codes, then language name map (English + native names), then base subtag. Unsupported languages fall back to English with a warning.

### 9. Focus Mode (Passive Recording)

A separate mode where recording is driven by terminal focus rather than key holds. When `focusMode: true`, recording starts on terminal focus and stops on blur. A 5-second silence timeout tears down idle connections. This enables a "multi-clauding army" workflow where voice input follows window focus. Currently hardcoded to `focusMode: false` in the REPL wiring.

## Configuration & Settings

### Settings Keys

| Key | Source | Type | Description |
|-----|--------|------|-------------|
| `voiceEnabled` | `settings.json` | `boolean` | Master toggle for voice mode |
| `language` | `settings.json` | `string` | STT language (BCP-47 code or language name) |
| `prefersReducedMotion` | `settings.json` | `boolean` | Disables shimmer animation, shows static "Voice: processing..." |

### Global Config Keys (persisted across sessions)

| Key | Type | Description |
|-----|------|-------------|
| `voiceNoticeSeenCount` | `number` | Times the "Voice mode is now available" banner has been shown (max 3) |
| `voiceLangHintShownCount` | `number` | Times the `/voice` dictation-language hint has been shown (max 2) |
| `voiceLangHintLastLanguage` | `string` | Last resolved STT language code (resets hint count on change) |
| `voiceFooterHintSeenCount` | `number` | Times the "hold Space to speak" footer hint has been shown (max 3) |

### Environment Variables

| Variable | Description |
|----------|-------------|
| `VOICE_STREAM_BASE_URL` | Override the WebSocket base URL (debug/testing) |
| `CLAUDE_CODE_REMOTE` | When truthy, disables voice (no local mic in remote environments) |

### Feature Flags (Compile-time)

| Flag | Purpose |
|------|---------|
| `VOICE_MODE` | Master gate — all voice code is dead-code-eliminated when false |

### Feature Flags (Runtime, GrowthBook)

| Flag | Default | Purpose |
|------|---------|---------|
| `tengu_amber_quartz_disabled` | `false` | Kill-switch — when `true`, voice is disabled globally |
| `tengu_cobalt_frost` | `false` | Enables Deepgram Nova 3 via conversation-engine routing |

### Keybinding

Default: `space` -> `voice:pushToTalk` in the `Chat` context. Rebindable via `keybindings.json`. Null-unbinding disables hold-to-talk.

## UI Components

### VoiceIndicator (`src/components/PromptInput/VoiceIndicator.tsx`)

Three states:
- **idle**: Returns null (invisible)
- **recording**: Shows `listening...` (dim text)
- **processing**: Shows `Voice: processing...` with a 2-second period sine-wave shimmer between gray (#999) and lighter gray (#B9B9B9). Falls back to static warning-colored text when `prefersReducedMotion` is set.

### VoiceWarmupHint

Shows `keep holding...` (dim, static text) during the warmup window (~120ms between rapid press #2 and activation threshold).

### VoiceModeNotice (`src/components/LogoV2/VoiceModeNotice.tsx`)

Startup banner: `* Voice mode is now available . /voice to enable`. Shown up to 3 times, only when voice is available but not yet enabled, and the Opus 1M merge notice is not showing.

### Cursor Waveform (`src/components/TextInput.tsx`)

During recording, the normal cursor is replaced with a single-bar waveform visualization built from the latest audio level. Uses Unicode block characters with 50ms animation frames. Disabled when `prefersReducedMotion` is set.

### Notifications Override (`src/components/PromptInput/Notifications.tsx`)

When voice is recording or processing, ALL normal notifications are replaced with just the `VoiceIndicator`. Voice errors show separately below when state returns to idle.

### Footer Hint (`src/components/PromptInput/PromptInputFooterLeftSide.tsx`)

Shows `hold Space to speak` (or the configured key) in the footer when voice is enabled, state is idle, no other hints are showing, and the hint has been shown fewer than 3 times.

## Platform Support

### macOS
- **Primary**: Native NAPI module (`cpal` -> CoreAudio/AudioUnit frameworks)
- **Fallback**: SoX `rec` command (requires `brew install sox`)
- **Mic permission**: TCC dialog triggered on first voice activation via probe recording
- **Gotcha**: `dlopen` of `audio-capture.node` blocks event loop for 1-8 seconds on cold CoreAudio daemon

### Windows
- **Primary**: Native NAPI module (`cpal` -> WASAPI)
- **Fallback**: None. If the native module fails, voice is unavailable.
- **Gotcha**: Bun's ws implementation can fire `unexpected-response` for successful 101 upgrades (claude-code#40510)

### Linux
- **Primary**: Native NAPI module (`cpal` -> ALSA), only if `/proc/asound/cards` shows sound cards
- **Fallback 1**: `arecord` (ALSA utils) — probed with a 150ms device-open test
- **Fallback 2**: SoX `rec` command
- **WSL**: WSL2+WSLg (Win11) works via PulseAudio RDP pipes. WSL1/Win10-WSL2 have no audio device — voice unavailable with specific error message.

### Remote / Homespace
Voice is disabled entirely when `isRunningOnHomespace()` or `CLAUDE_CODE_REMOTE` is truthy. Error: "Voice mode requires microphone access, but no audio device is available in this environment."

## Analytics Events

| Event | Trigger |
|-------|---------|
| `tengu_voice_toggled` | `/voice` command toggles voice on/off |
| `tengu_voice_recording_started` | Recording session begins (includes language, focus mode, locale) |
| `tengu_voice_recording_completed` | Recording session ends (includes transcript chars, duration, hadAudioSignal, retried, wsConnected, focusTriggered) |
| `tengu_voice_stream_early_retry` | First WebSocket error triggers a retry |
| `tengu_voice_silent_drop_replay` | Silent-drop detected, replaying audio on fresh connection |

## Edge Cases & Gotchas

1. **No TTS / voice output**: The system is input-only (STT). There is no text-to-speech or audio playback. "Voice mode" means voice INPUT, not voice conversation.

2. **Space key conflict**: Space is both the default voice trigger AND a typing character. The system differentiates via auto-repeat speed: normal typing has >120ms gaps, held space has 30-80ms gaps. First 2 rapid spaces flow through to input, then subsequent ones are swallowed, and all flow-through chars are stripped on activation.

3. **CJK full-width space**: When the hold key is space, U+3000 (full-width space from CJK IMEs) is also accepted as the trigger key.

4. **Buffer copy requirement**: Audio chunks from the native NAPI module may share a pooled ArrayBuffer. Both the recording callback and the WebSocket send path create owned copies via `Buffer.from()` to prevent stale/overlapping memory reads.

5. **Deferred CloseStream**: The `CloseStream` message is sent via `setTimeout(0)` to ensure any queued audio callbacks from the native module are flushed to the WebSocket first.

6. **OAuth-only**: Voice requires Anthropic OAuth tokens (Claude.ai account). The endpoint is not accessible with API keys. The availability check verifies both the auth provider AND the existence of an access token.

7. **Cloudflare bypass**: The WebSocket connects to `api.anthropic.com` (not `claude.ai`) because Claude.ai's Cloudflare zone uses TLS fingerprinting that blocks non-browser clients. Same backend, different CF zone.

8. **Kill-switch cache**: `getFeatureValue_CACHED_MAY_BE_STALE` reads from a disk cache. Fresh installs with no cache default to "not killed" (voice enabled). The cache may be stale — this is intentional so voice works immediately without waiting for GrowthBook init.

9. **Zombie session prevention**: Each recording session gets a generation counter (`sessionGenRef`). All callbacks check staleness before mutating state, preventing abandoned slow-connecting WebSockets from corrupting the current session.

10. **Early-error retry**: The first WebSocket error before any transcript triggers a single automatic retry with 250ms backoff. This handles transient CE-pod collisions and Deepgram teardown-window failures.

11. **Audio buffering during connect**: Recording starts immediately (audio goes to `audioBuffer[]`). When the WebSocket connects, buffered chunks are flushed in ~1s coalesced slices (32KB target per frame).

12. **Voice command availability**: The `/voice` command has `availability: ['claude-ai']` — it only appears for Claude.ai authenticated users. It's hidden (but still enabled) when the runtime kill-switch is on, preventing the command from leaking voice-specific strings.

## Cross-References

| Report | Relevance |
|--------|-----------|
| [09 — Hooks System](09_hooks_system.md) | useVoice, useVoiceEnabled, useVoiceIntegration are major hooks |
| [21 — Feature Flags](21_feature_flags.md) | `VOICE_MODE` is a primary feature flag with compile-time DCE |
| [18 — Commands Catalog](18_commands_catalog.md) | `/voice` command registration and gating |
| [06 — Tool Pipeline](06_tool_pipeline.md) | ConfigTool integration for `voiceEnabled` setting |

---

*Generated by Claude Opus 4.6 (1M context) — 2026-04-05*
