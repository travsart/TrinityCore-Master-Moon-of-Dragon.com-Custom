---
description: "teleport CCR — remote session management, SSH tunnel creation, remote machine execution, cloud compute offloading, cross-machine session transfer"
---

# Teleport (Remote Sessions / CCR) -- Arcanum Wiki

## What Is This?

Teleport is Claude Code's system for creating and managing remote cloud sessions via the Sessions API (CCR -- Claude Code Runner). It enables features like UltraPlan by launching sandboxed containers in Anthropic's cloud, seeding them with your local repository, and communicating via a polling-based event protocol. The teleport module handles authentication, session lifecycle, git bundle creation/upload, environment selection, and event streaming.

## How It Works

### API Layer (api.ts)

The API module provides authenticated REST communication with the Sessions API at `{BASE_API_URL}/v1/sessions`. All requests require Anthropic OAuth tokens and an organization UUID.

Key operations:
- **Create sessions**: POST to `/v1/sessions` with context (git sources, cwd, outcomes, system prompt, model)
- **Fetch sessions**: GET single or list (with transforms from SessionResource to CodeSession format)
- **Send events**: POST user messages to `/v1/sessions/{id}/events` with 30s timeout for cold-start containers
- **Update titles**: PATCH session titles
- **Poll events**: Via `pollRemoteSessionEvents()` (imported from teleport.ts)

Retry logic uses exponential backoff: 2s, 4s, 8s, 16s (4 retries = 5 total attempts). Only transient errors (network failures, 5xx) are retried; 4xx errors are not.

```typescript
export const CCR_BYOC_BETA = 'ccr-byoc-2025-07-29'
```

The `anthropic-beta: ccr-byoc-2025-07-29` header is sent on all session requests, indicating this uses the BYOC (Bring Your Own Compute) beta API.

### Git Bundle Seeding (gitBundle.ts)

When launching a remote session, Teleport can seed the container with your local repository state via a git bundle upload. The process:

1. **Stash WIP**: `git stash create` captures uncommitted changes as a dangling commit, referenced via `refs/seed/stash`
2. **Bundle with fallback chain**: `--all` (full repo) -> `HEAD` (current branch only) -> squashed-root (single parentless commit with just the tree snapshot)
3. **Size limits**: Default 100MB max (`tengu_ccr_bundle_max_bytes` GrowthBook config), with each fallback tier creating a smaller bundle
4. **Upload**: Via Files API, with fixed relative path `_source_seed.bundle`
5. **Cleanup**: Always deletes the temp bundle file and seed refs, even on failure

```typescript
const DEFAULT_BUNDLE_MAX_BYTES = 100 * 1024 * 1024
type BundleScope = 'all' | 'head' | 'squashed'
```

The squashed-root tier is a last resort: it creates a single parentless commit of HEAD's tree (or the stash tree if WIP exists), discarding all history. The receiver needs special `refs/seed/root` handling.

Stale refs from crashed prior runs are swept before bundling to prevent `--all` from including garbage.

### Environment Selection (environmentSelection.ts)

Users can configure which cloud environment to use for remote sessions. Environments come in three kinds: `anthropic_cloud`, `byoc`, and `bridge`.

The selection cascade:
1. Check merged settings for `remote.defaultEnvironmentId`
2. If found, look up the matching environment from the API
3. Track which settings source (user, project, enterprise) configured it
4. Default: first non-bridge environment

### Environments API (environments.ts)

Manages cloud environments via `/v1/environment_providers`:
- List available environments
- Create default `anthropic_cloud` environments for users who have none

Default cloud environment config:
```typescript
config: {
  environment_type: 'anthropic',
  cwd: '/home/user',
  languages: [
    { name: 'python', version: '3.11' },
    { name: 'node', version: '20' },
  ],
  network_config: {
    allowed_hosts: [],
    allow_default_hosts: true,
  },
}
```

## Feature Gating

- Requires Anthropic OAuth authentication (not API keys)
- Organization UUID required
- BYOC beta header required on all requests
- Bundle size controlled by `tengu_ccr_bundle_max_bytes` GrowthBook config

## User-Facing Behavior

Teleport is typically invoked indirectly through features like UltraPlan or the `/remote` command. The user experience:
1. Local repo is bundled (including uncommitted changes) and uploaded
2. A cloud container is provisioned with the repo contents
3. Claude runs in the container, visible through the browser or polled from the terminal
4. Results (git branches, PRs) are pushed back to the repo

## Key Source Files

| File | Purpose |
|------|---------|
| `src/utils/teleport/api.ts` | Sessions API client, retry logic, event sending |
| `src/utils/teleport/gitBundle.ts` | Git bundle creation, fallback chain, Files API upload |
| `src/utils/teleport/environmentSelection.ts` | Environment preference resolution |
| `src/utils/teleport/environments.ts` | Environment API client, default creation |

## Configuration

- `remote.defaultEnvironmentId` in settings (user/project/enterprise level)
- `tengu_ccr_bundle_max_bytes` GrowthBook config (default 100MB)
- `BASE_API_URL` from OAuth config

## Interesting Findings

1. **The three-tier bundle fallback** (all -> HEAD -> squashed) is an elegant solution for large repos. A 5GB monorepo would fail the 100MB limit with `--all`, but a squashed snapshot of just the current tree might fit.

2. **WIP capture is sophisticated**: `git stash create` makes a dangling commit without touching the working tree or refs/stash, and it is baked into the squashed tree when history stripping is needed. Untracked files are intentionally excluded.

3. **Session outcomes include git info**: The API returns `OutcomeGitInfo` with repo and branch names, meaning the remote session can push branches back to GitHub as a deliverable.

4. **The BYOC beta** (`ccr-byoc-2025-07-29`) suggests this evolved from a "bring your own compute" feature where enterprises could provide their own cloud infrastructure for running sessions.

5. **The `seed_bundle_file_id` field** in SessionContext shows that bundle seeding is an official part of the Sessions API, not a hack -- the container infrastructure knows how to unpack git bundles.
