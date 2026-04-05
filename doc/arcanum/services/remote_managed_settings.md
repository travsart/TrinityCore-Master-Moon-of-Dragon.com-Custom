---
description: "remote managed settings — enterprise MDM policy enforcement, managed-only controls, remote settings sync, organization security policies"
---

# Remote Managed Settings Service -- Arcanum Wiki

## What Is This?

Remote Managed Settings handles enterprise/organization-managed configuration that administrators push to Claude Code installations. This is the mechanism by which IT departments and org admins can enforce policies -- such as allowed models, required permission modes, blocked tools, or custom system prompts -- across all Claude Code users in their organization.

## How It Works

The service fetches managed settings from the Anthropic API and applies them with higher priority than user-local settings. Enterprise settings override user preferences when conflicts exist.

The `index.ts` module coordinates fetching and `types.ts` defines the schema for managed settings payloads. Settings are cached locally and refreshed periodically, similar to other sync services.

Managed settings typically control:
- Allowed/blocked models
- Permission mode restrictions
- Tool access policies
- Custom system prompt injections (enterprise CLAUDE.md)
- Feature gate overrides

## Key Source Files

| File | Purpose |
|------|---------|
| `index.ts` | Fetch and apply managed settings |
| `types.ts` | Schema definitions for managed payloads |

## Configuration

- Requires enterprise OAuth subscription
- Settings fetched from org-scoped API endpoints
- Cached locally with periodic refresh
- Enterprise settings take priority over user settings

## Interesting Findings

1. **Enterprise CLAUDE.md injection** is handled through this pathway -- org admins can push instructions that appear in every Claude Code session for their users, above user-level settings.

2. **The priority chain** is: remote managed (enterprise) > user settings > defaults. This ensures compliance policies cannot be overridden by individual users.
