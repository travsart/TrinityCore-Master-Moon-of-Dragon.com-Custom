---
description: "permission evaluation order — 8 rule sources priority, managed > project > user, hook allow vs deny rules, defense-in-depth"
---

# Permission Evaluation Order
> Source: `utils/permissions/`, `components/permissions/`
> Status: STUB — needs research

## Known (from Tier 2)
- 7 modes, 8 rule sources
- YOLO classifier for auto mode
- Hook allow does NOT bypass deny rules

## Key Questions
- Complete evaluation order diagram
- Rule source priority: managed > project > user > defaults?
- How does auto mode decide? What's the YOLO classifier threshold?
- Enterprise managed-only controls
