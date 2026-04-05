---
description: "model config commands — /model /fast /effort /output-style /color /theme /advisor /config /brief, model selection, fast mode, effort levels"
---

# Model & Configuration Commands -- Arcanum Wiki

## Overview

These commands control which AI model is used, inference parameters like effort level and speed, and visual presentation settings like colors and themes. They modify runtime behavior without touching files on disk (except for persisted settings).

## Commands

### /model
- **Arguments**: `[model]`
- **What it does**: Sets the AI model used for the main conversation loop. Without arguments, shows an interactive model picker. With an argument, attempts to set the model directly. Displays the currently active model in its description.
- **Feature gating**: Always available. The `immediate` property is dynamic -- it calls `shouldInferenceConfigCommandBeImmediate()` which returns true when arguments are provided (so `/model sonnet` executes instantly, but `/model` alone shows a picker).
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'model',
  get description() {
    return `Set the AI model for Claude Code (currently ${renderModelName(getMainLoopModel())})`
  },
  argumentHint: '[model]',
  get immediate() {
    return shouldInferenceConfigCommandBeImmediate()
  },
}
```
- **Notes**: The description is dynamic -- it always shows the currently active model name.

---

### /fast
- **Arguments**: `[on|off]`
- **What it does**: Toggles "fast mode" which restricts the model to a faster, cheaper model (displayed via `FAST_MODE_MODEL_DISPLAY` constant). This is a convenience toggle for quickly switching between high-quality and fast responses.
- **Feature gating**: Only available for `claude-ai` and `console` availability contexts. Controlled by `isFastModeEnabled()` -- both the `isEnabled` and `isHidden` properties check this gate. The command is completely invisible when fast mode is not enabled for the user.
- **Key code**:
```typescript
const fast = {
  type: 'local-jsx',
  name: 'fast',
  get description() {
    return `Toggle fast mode (${FAST_MODE_MODEL_DISPLAY} only)`
  },
  availability: ['claude-ai', 'console'],
  isEnabled: () => isFastModeEnabled(),
  get isHidden() {
    return !isFastModeEnabled()
  },
  argumentHint: '[on|off]',
}
```

---

### /effort
- **Arguments**: `[low|medium|high|max|auto]`
- **What it does**: Sets the effort level for model inference. This controls how much "thinking" the model does, mapping to the `reasoning_effort` or `budget_tokens` API parameter. Lower effort = faster/cheaper responses; higher effort = more thorough reasoning.
- **Feature gating**: None -- always available. Like `/model`, the `immediate` property is dynamic.
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'effort',
  description: 'Set effort level for model usage',
  argumentHint: '[low|medium|high|max|auto]',
  get immediate() {
    return shouldInferenceConfigCommandBeImmediate()
  },
}
```
- **Notes**: The `auto` setting lets Claude Code dynamically choose effort based on task complexity.

---

### /output-style
- **Arguments**: None documented
- **What it does**: DEPRECATED. Previously allowed changing the output style (e.g., markdown rendering, verbosity). Now redirects users to `/config` for output style changes.
- **Feature gating**: `isHidden: true` -- never shown in help or autocomplete.
- **Key code**:
```typescript
const outputStyle = {
  type: 'local-jsx',
  name: 'output-style',
  description: 'Deprecated: use /config to change output style',
  isHidden: true,
}
```
- **Notes**: This is a deprecation shim -- it still works but directs users to the newer `/config` panel.

---

### /color
- **Arguments**: `<color|default>`
- **What it does**: Sets the prompt bar color for the current session. This is purely cosmetic and does not persist between sessions. Useful for visually distinguishing multiple terminal tabs.
- **Feature gating**: None -- always available.
- **Execution**: `immediate: true` -- executes instantly.
- **Key code**:
```typescript
const color = {
  type: 'local-jsx',
  name: 'color',
  description: 'Set the prompt bar color for this session',
  immediate: true,
  argumentHint: '<color|default>',
}
```

---

### /theme
- **Arguments**: None (interactive picker)
- **What it does**: Opens an interactive theme picker to change the visual theme of Claude Code. Themes affect syntax highlighting, colors, and overall appearance.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const theme = {
  type: 'local-jsx',
  name: 'theme',
  description: 'Change the theme',
}
```

---

### /advisor
- **Arguments**: `[<model>|off]`
- **What it does**: Configures an "advisor" model -- a secondary model that reviews and advises on the primary model's output. Without arguments, shows the current advisor status. Accepts a model name to set, or `off`/`unset` to disable.
- **Feature gating**: Gated behind `canUserConfigureAdvisor()`. Hidden when the user cannot configure advisors.
- **Validation**: Checks that the specified model is valid (`validateModel`), is a valid advisor model (`isValidAdvisorModel`), and that the current base model supports advisors (`modelSupportsAdvisor`). Persists the setting via `updateSettingsForSource`.
- **Key code**:
```typescript
const advisor = {
  type: 'local',
  name: 'advisor',
  description: 'Configure the advisor model',
  argumentHint: '[<model>|off]',
  isEnabled: () => canUserConfigureAdvisor(),
  get isHidden() {
    return !canUserConfigureAdvisor()
  },
  supportsNonInteractive: true,
}
```
- **Notes**: When the advisor is set but the current base model does not support it, the status shows "(inactive)".

---

### /config
- **Aliases**: `/settings`
- **Arguments**: None
- **What it does**: Opens an interactive configuration panel where users can modify various settings including output style, model preferences, and other options. This is the centralized settings UI.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const config = {
  aliases: ['settings'],
  type: 'local-jsx',
  name: 'config',
  description: 'Open config panel',
}
```

---

### /brief
- **Arguments**: None (toggle)
- **What it does**: Toggles "brief-only mode" where the model uses a special Brief tool (`SendUserMessage`) for all responses instead of raw text. When enabled, responses are structured through the brief tool. Toggling changes the tool list available to the model.
- **Feature gating**: Triple-gated:
  1. Build-time: `feature('KAIROS')` or `feature('KAIROS_BRIEF')` must be true
  2. Remote config: `tengu_kairos_brief_config.enable_slash_command` must be true
  3. Runtime: `isBriefEntitled()` must be true to enable (disabling is always allowed)
- **Key code**:
```typescript
const brief = {
  type: 'local-jsx',
  name: 'brief',
  description: 'Toggle brief-only mode',
  isEnabled: () => {
    if (feature('KAIROS') || feature('KAIROS_BRIEF')) {
      return getBriefConfig().enable_slash_command
    }
    return false
  },
  immediate: true,
}
```
- **Notes**: Toggling brief mode invalidates prompt cache because the tool list changes. When Kairos is active, the system prompt already mandates `SendUserMessage`, so the meta-message injection is skipped.

## Hidden/Undocumented Commands

- **/output-style** -- Deprecated, permanently hidden but still functional. Redirects to `/config`.
- **/advisor** -- Hidden from most users; only visible when `canUserConfigureAdvisor()` returns true.
- **/brief** -- Hidden from most users; requires multiple feature gates to be active.
- **/fast** -- Hidden when `isFastModeEnabled()` is false.
