---
description: "skill loading pipeline — 8 sources, bundled project user dynamic, 6 cache layers, loading order, cache invalidation, skill frontmatter YAML"
---

# Skill Loading Pipeline
> Source: `src/skills/`, `utils/skills/`
> Status: STUB — needs research

## Known (from Tier 2)
- 8 sources, 17 bundled skills
- Dynamic discovery from file edits
- Conditional `paths` activation
- 6 cache layers

## Key Questions
- Complete loading order: bundled → project → user → dynamic?
- How does `paths:` frontmatter trigger activation?
- Cache invalidation — when do skills get reloaded?
- The file edit walk-up search for `.claude/skills/`
