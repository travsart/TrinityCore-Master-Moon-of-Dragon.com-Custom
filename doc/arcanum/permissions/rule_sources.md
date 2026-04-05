---
description: "permission rule sources — managed settings user project local CLI hooks YOLO defaults, 8 sources merge override order, active rule inspection"
---

# Permission Rule Sources
> Source: `utils/permissions/`
> Status: STUB — needs research

## Known: 8 Rule Sources
1. Managed settings (enterprise)
2. User settings
3. Project settings
4. Local settings
5. CLI flags
6. Hook results
7. YOLO classifier
8. Permission mode default

## Key Questions
- Exact merge/override order
- What each source can set (allow, deny, ask, specific tools)
- How to inspect active rules at runtime
