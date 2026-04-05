---
description: "voice mode — push-to-talk STT streaming, voice keyterms, speech-to-text input, hands-free interaction, audio input processing"
---

# Voice Mode / Push-to-Talk -- Arcanum Wiki

## What Is This?

Voice Mode is an unreleased feature that enables spoken interaction with Claude Code. The source reveals it requires Anthropic OAuth authentication (not API keys) and connects to a `voice_stream` endpoint on claude.ai. It is gated behind both a build-time `VOICE_MODE` feature flag and a GrowthBook kill-switch called `tengu_amber_quartz_disabled`.

The implementation visible in the external source is minimal -- just the gating logic in `voiceModeEnabled.ts`. The actual voice streaming, recording, and playback code lives in internal-only files that are not present in the external build.

## How It Works

Three layers of checks determine voice mode availability (voiceModeEnabled.ts):

```typescript
export function isVoiceModeEnabled(): boolean {
  return hasVoiceAuth() && isVoiceGrowthBookEnabled()
}
```

**Auth check** (`hasVoiceAuth()`):
1. Must be using Anthropic OAuth (not API keys, Bedrock, Vertex, or Foundry)
2. Must have a valid access token (checked via `getClaudeAIOAuthTokens()`)
3. Uses the memoized token getter -- first call spawns macOS `security` (~20-50ms), subsequent calls are cache hits

**GrowthBook check** (`isVoiceGrowthBookEnabled()`):
1. Build-time: `feature('VOICE_MODE')` must be enabled
2. Runtime: `tengu_amber_quartz_disabled` flag must NOT be true
3. Default is `false` (not killed), so fresh installs with no GrowthBook cache get voice working immediately

The positive ternary pattern is deliberate: `feature('VOICE_MODE') ? !getFeatureValue(...) : false`. A negative pattern would fail to eliminate inline string literals from external builds.

## Feature Gating

| Gate | Type | Default | Notes |
|------|------|---------|-------|
| `VOICE_MODE` | Build-time feature flag | Off in external | Compiled out of npm builds |
| `tengu_amber_quartz_disabled` | GrowthBook kill-switch | `false` | Emergency off switch |
| Anthropic OAuth | Runtime auth check | Required | No voice with API keys |
| OAuth access token | Runtime token check | Required | Must be logged in |

## User-Facing Behavior

Voice mode is not visible in the external Claude Code build. If it were enabled, the `/voice` command would activate push-to-talk, and `useVoiceEnabled()` would control UI rendering. The feature presumably enables speaking to Claude and hearing responses.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/voice/voiceModeEnabled.ts` | Kill-switch and auth gating (55 lines) |

## Configuration

- No user-configurable settings in the external build
- Controlled entirely by build flags and server-side GrowthBook

## Interesting Findings

1. **The codename "tengu"** appears throughout the codebase as a prefix for GrowthBook flags. This is likely an internal project codename for Claude Code.

2. **Voice requires OAuth specifically** because it uses the `voice_stream` endpoint on claude.ai, which is only available through the web authentication flow -- not the API key system.

3. **The kill-switch defaults to "not killed"** (`false`), which is the safe default for deployment: if GrowthBook is down or the cache is stale, voice stays available rather than being accidentally disabled.
