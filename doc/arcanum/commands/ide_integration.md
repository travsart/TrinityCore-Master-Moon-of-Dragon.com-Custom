---
description: "IDE integration commands — /remote-control /ide /desktop /chrome /mobile /terminal-setup /web-setup /remote-env, VS Code bridge, browser automation"
---

# IDE Integration Commands -- Arcanum Wiki

## Overview

These commands manage Claude Code's integration with external environments -- IDEs, desktop applications, browsers, mobile apps, and terminal configuration. They handle the bridge between Claude Code's CLI interface and graphical environments.

## Commands

### /remote-control
- **Aliases**: `/rc`
- **Arguments**: `[name]`
- **What it does**: Connects the current terminal for remote-control sessions. This enables the "bridge" system where a remote environment (like Claude's web interface) can control this terminal instance. The bridge allows real-time bidirectional communication.
- **Feature gating**: Double-gated:
  1. Build-time: `feature('BRIDGE_MODE')` must be true
  2. Runtime: `isBridgeEnabled()` must return true
  Hidden when not enabled.
- **Execution**: `immediate: true`
- **Key code**:
```typescript
const bridge = {
  type: 'local-jsx',
  name: 'remote-control',
  aliases: ['rc'],
  description: 'Connect this terminal for remote-control sessions',
  argumentHint: '[name]',
  isEnabled,
  get isHidden() {
    return !isEnabled()
  },
  immediate: true,
}
```
- **Notes**: The directory is named `bridge/` but the command is registered as `/remote-control`. This is the client side of the remote control architecture -- the server side runs in Claude's web interface.

---

### /ide
- **Arguments**: `[open]`
- **What it does**: Manages IDE integrations and shows their status. Can display which IDEs are connected, their health status, and configuration. The `open` argument launches the IDE integration setup.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const ide = {
  type: 'local-jsx',
  name: 'ide',
  description: 'Manage IDE integrations and show status',
  argumentHint: '[open]',
}
```

---

### /desktop
- **Aliases**: `/app`
- **Arguments**: None
- **What it does**: Transfers the current session to Claude Desktop (the Electron app). Generates a handoff that allows continuing the conversation in the desktop application with its richer UI.
- **Feature gating**: Only available for `claude-ai` users. Platform-restricted to macOS (all architectures) and Windows x64. Hidden on unsupported platforms.
- **Key code**:
```typescript
function isSupportedPlatform(): boolean {
  if (process.platform === 'darwin') return true
  if (process.platform === 'win32' && process.arch === 'x64') return true
  return false
}

const desktop = {
  type: 'local-jsx',
  name: 'desktop',
  aliases: ['app'],
  description: 'Continue the current session in Claude Desktop',
  availability: ['claude-ai'],
  isEnabled: isSupportedPlatform,
  get isHidden() {
    return !isSupportedPlatform()
  },
}
```
- **Notes**: Linux is not currently supported for the desktop handoff.

---

### /chrome
- **Arguments**: None
- **What it does**: Configures "Claude in Chrome" settings (Beta). This manages the Chrome extension integration that allows Claude to interact with web pages.
- **Feature gating**: Only available for `claude-ai` users. Disabled in non-interactive sessions.
- **Key code**:
```typescript
const command: Command = {
  name: 'chrome',
  description: 'Claude in Chrome (Beta) settings',
  availability: ['claude-ai'],
  isEnabled: () => !getIsNonInteractiveSession(),
  type: 'local-jsx',
}
```

---

### /mobile
- **Aliases**: `/ios`, `/android`
- **Arguments**: None
- **What it does**: Shows a QR code that links to the Claude mobile app download page. A simple convenience command for quickly getting the mobile app.
- **Feature gating**: None -- always available.
- **Key code**:
```typescript
const mobile = {
  type: 'local-jsx',
  name: 'mobile',
  aliases: ['ios', 'android'],
  description: 'Show QR code to download the Claude mobile app',
}
```

---

### /terminal-setup
- **Arguments**: None
- **What it does**: Installs key binding configurations for the current terminal. On most terminals, this sets up the Shift+Enter key binding for inserting newlines. On Apple Terminal specifically, it configures Option+Enter and visual bell settings.
- **Feature gating**: Hidden on terminals that natively support CSI u / Kitty keyboard protocol (Ghostty, Kitty, iTerm2, WezTerm) since those terminals don't need special setup.
- **Key code**:
```typescript
const NATIVE_CSIU_TERMINALS: Record<string, string> = {
  ghostty: 'Ghostty',
  kitty: 'Kitty',
  'iTerm.app': 'iTerm2',
  WezTerm: 'WezTerm',
}

const terminalSetup = {
  type: 'local-jsx',
  name: 'terminal-setup',
  description: env.terminal === 'Apple_Terminal'
    ? 'Enable Option+Enter key binding for newlines and visual bell'
    : 'Install Shift+Enter key binding for newlines',
  isHidden: env.terminal !== null && env.terminal in NATIVE_CSIU_TERMINALS,
}
```
- **Notes**: The description is dynamic based on the detected terminal. This addresses a common pain point where users cannot insert newlines in their prompts.

---

### /web-setup
- **Arguments**: None
- **What it does**: Sets up Claude Code on the web, which requires connecting a GitHub account. This is the onboarding flow for the web-based Claude Code environment.
- **Feature gating**: Triple-gated:
  1. Only for `claude-ai` users
  2. GrowthBook feature flag: `tengu_cobalt_lantern`
  3. Policy: `allow_remote_sessions`
  Hidden when remote sessions are not policy-allowed.
- **Key code**:
```typescript
const web = {
  type: 'local-jsx',
  name: 'web-setup',
  description: 'Setup Claude Code on the web ' +
    '(requires connecting your GitHub account)',
  availability: ['claude-ai'],
  isEnabled: () =>
    getFeatureValue_CACHED_MAY_BE_STALE('tengu_cobalt_lantern', false) &&
    isPolicyAllowed('allow_remote_sessions'),
}
```
- **Notes**: The directory is named `remote-setup/` but the command is registered as `/web-setup`.

---

### /remote-env
- **Arguments**: None
- **What it does**: Configures the default remote environment for teleport sessions. Teleport is the feature that moves a local session to a cloud-based environment for tasks requiring more resources.
- **Feature gating**: Requires Claude AI subscriber status AND `allow_remote_sessions` policy. Hidden when either condition is not met.
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'remote-env',
  description: 'Configure the default remote environment for teleport sessions',
  isEnabled: () =>
    isClaudeAISubscriber() && isPolicyAllowed('allow_remote_sessions'),
  get isHidden() {
    return !isClaudeAISubscriber() || !isPolicyAllowed('allow_remote_sessions')
  },
}
```

## Hidden/Undocumented Commands

- **/remote-control** (alias `/rc`) -- Hidden when bridge mode is not enabled; most users never see it.
- **/web-setup** -- Gated behind a GrowthBook flag (`tengu_cobalt_lantern`); only visible during rollout.
- **/remote-env** -- Only visible to Claude AI subscribers with remote session policy.
- **/terminal-setup** -- Automatically hidden on modern terminals that don't need it.
