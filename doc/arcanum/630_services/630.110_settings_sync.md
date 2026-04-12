---
description: "settings sync service — cross-device settings synchronization, cloud settings backup, settings migration between machines"
title: "Settings Sync Service -- Arcanum Wiki"
tags: [services, cloud-settings]
---

# Settings Sync Service -- Arcanum Wiki

## What Is This?

The Settings Sync service synchronizes Claude Code settings between the local configuration and a remote server. It ensures that user preferences (like model selection, permission mode defaults, output style) persist across devices and Claude Code installations.

## How It Works

The service operates through a cache-based sync mechanism:

- **`syncCache.ts`** -- Manages the local cache of synced settings, reading from and writing to the global config file
- **`syncCacheState.ts`** -- Tracks the sync state (last sync timestamp, dirty flags) to determine when a sync is needed
- **`securityCheck.tsx`** -- Validates settings before applying them, ensuring security-sensitive settings (like permission mode) are not unexpectedly changed by sync
- **`types.ts`** -- Type definitions for synced settings payloads
- **`index.ts`** -- Orchestration of the sync flow

Settings are synced using the same OAuth authentication as other API calls. The sync is non-blocking -- stale local values are used until the background sync completes.

## Key Source Files

| File | Purpose |
|------|---------|
| `index.ts` | Sync orchestration |
| `syncCache.ts` | Local cache management |
| `syncCacheState.ts` | Sync state tracking |
| `securityCheck.tsx` | Security validation of synced settings |
| `types.ts` | Type definitions |

## Configuration

- Uses OAuth authentication for API communication
- Settings changes trigger background sync
- Security-sensitive settings validated before application

## Interesting Findings

1. **Security checks are applied before synced settings take effect**, preventing a compromised server from pushing dangerous permission mode changes to the client.

2. **The sync is designed to be non-blocking** -- Claude Code starts with cached local settings and updates in the background, so startup is never delayed by network latency.
