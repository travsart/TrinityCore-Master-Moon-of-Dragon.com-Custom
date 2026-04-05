---
description: "account auth commands — /login /logout /oauth-refresh /install-github-app /install-slack-app /upgrade /passes, OAuth flow, API key setup"
---

# Account & Authentication Commands -- Arcanum Wiki

## Overview

These commands handle authentication, account management, and app installations. They manage the lifecycle of user credentials and third-party integrations (GitHub, Slack).

## Commands

### /login
- **Arguments**: None
- **What it does**: Signs in with an Anthropic account. The description dynamically changes based on current auth state:
  - If already authenticated with an API key: "Switch Anthropic accounts"
  - If not authenticated: "Sign in with your Anthropic account"

  Opens an OAuth flow in the browser for authentication.
- **Feature gating**: Can be disabled via `DISABLE_LOGIN_COMMAND` environment variable.
- **Key code**:
```typescript
export default () => ({
  type: 'local-jsx',
  name: 'login',
  description: hasAnthropicApiKeyAuth()
    ? 'Switch Anthropic accounts'
    : 'Sign in with your Anthropic account',
  isEnabled: () => !isEnvTruthy(process.env.DISABLE_LOGIN_COMMAND),
})
```
- **Notes**: This is exported as a factory function (not a static object) so the description is evaluated fresh each time the command list is built. This is unique among commands.

---

### /logout
- **Arguments**: None
- **What it does**: Signs out from the current Anthropic account. Clears stored credentials.
- **Feature gating**: Can be disabled via `DISABLE_LOGOUT_COMMAND` environment variable.
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'logout',
  description: 'Sign out from your Anthropic account',
  isEnabled: () => !isEnvTruthy(process.env.DISABLE_LOGOUT_COMMAND),
}
```

---

### /oauth-refresh
- **Arguments**: Unknown (stubbed)
- **What it does**: STUBBED OUT. Was likely used for manually refreshing OAuth tokens.
- **Feature gating**: `isEnabled: () => false`, `isHidden: true`

---

### /install-github-app
- **Arguments**: None
- **What it does**: Sets up Claude GitHub Actions for a repository. This installs the GitHub App that enables Claude to interact with pull requests, issues, and CI/CD workflows. Walks the user through the GitHub App installation flow.
- **Feature gating**: Available only for `claude-ai` and `console` availability contexts. Can be disabled via `DISABLE_INSTALL_GITHUB_APP_COMMAND` environment variable.
- **Key code**:
```typescript
const installGitHubApp = {
  type: 'local-jsx',
  name: 'install-github-app',
  description: 'Set up Claude GitHub Actions for a repository',
  availability: ['claude-ai', 'console'],
  isEnabled: () => !isEnvTruthy(process.env.DISABLE_INSTALL_GITHUB_APP_COMMAND),
}
```

---

### /install-slack-app
- **Arguments**: None
- **What it does**: Installs the Claude Slack app. Provides the setup flow for connecting Claude to a Slack workspace.
- **Feature gating**: Only available for `claude-ai` users. Not available in non-interactive mode.
- **Key code**:
```typescript
const installSlackApp = {
  type: 'local',
  name: 'install-slack-app',
  description: 'Install the Claude Slack app',
  availability: ['claude-ai'],
  supportsNonInteractive: false,
}
```

---

### /upgrade
- **Arguments**: None
- **What it does**: Shows the upgrade page for Claude Max plan, which offers higher rate limits and more Opus model access. Opens the subscription management interface.
- **Feature gating**: Only available for `claude-ai` users. Disabled for enterprise subscribers (they have different upgrade paths). Can be disabled via `DISABLE_UPGRADE_COMMAND` environment variable.
- **Key code**:
```typescript
const upgrade = {
  type: 'local-jsx',
  name: 'upgrade',
  description: 'Upgrade to Max for higher rate limits and more Opus',
  availability: ['claude-ai'],
  isEnabled: () =>
    !isEnvTruthy(process.env.DISABLE_UPGRADE_COMMAND) &&
    getSubscriptionType() !== 'enterprise',
}
```

---

### /passes
- **Arguments**: None
- **What it does**: Shows a referral interface for sharing free weeks of Claude Code with friends. The description dynamically changes:
  - With referrer reward: "Share a free week of Claude Code with friends and earn extra usage"
  - Without referrer reward: "Share a free week of Claude Code with friends"
- **Feature gating**: Hidden when the user is not eligible for passes (`checkCachedPassesEligibility()`) or when eligibility data has not been cached yet.
- **Key code**:
```typescript
export default {
  type: 'local-jsx',
  name: 'passes',
  get description() {
    const reward = getCachedReferrerReward()
    if (reward) {
      return 'Share a free week of Claude Code with friends and earn extra usage'
    }
    return 'Share a free week of Claude Code with friends'
  },
  get isHidden() {
    const { eligible, hasCache } = checkCachedPassesEligibility()
    return !eligible || !hasCache
  },
}
```

## Hidden/Undocumented Commands

- **/oauth-refresh** -- Stubbed out, permanently disabled. Dead code.
- **/passes** -- Hidden until eligibility is confirmed and cached. Most users may not see this.
