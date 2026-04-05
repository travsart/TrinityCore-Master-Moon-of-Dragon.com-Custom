---
description: "hook permission race — 4-way decision merge, allow does NOT override deny, defense-in-depth, most restrictive wins, 8 rule sources"
---

# Hook Permission Race
> Source: `hooks/toolPermission/`
> Status: STUB — needs research

## Known Finding
Hook `allow` does NOT bypass deny/ask rules (defense-in-depth). Hooks are additive only.

## Key Questions
- Exact evaluation order: hook result vs permission mode vs rule sources
- What happens when hook says allow but YOLO classifier says deny?
- Can hooks downgrade from allow to ask? From ask to deny?
