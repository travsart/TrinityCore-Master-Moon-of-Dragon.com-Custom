# MCP + Serena Research Protocol

**Version:** 1.0
**Last Updated:** 2025-10-29
**Status:** MANDATORY for all TrinityCore research

---

## Overview

When working on the TrinityCore Playerbot Integration project, you have access to TWO complementary research tools that MUST be used together for complete understanding. Using only one tool results in incomplete, low-quality research.

---

## Tool Responsibilities

### 🎮 TrinityCore MCP Server (Game Knowledge)

**What It Provides:**
- World of Warcraft game mechanics context
- Spell IDs, creature entries, zone IDs, quest IDs
- DBC/DB2/database data cross-references
- Curated workflows and best practices
- Common pitfalls and anti-patterns
- Game-specific domain knowledge

**Available Tools:**
```typescript
mcp__trinitycore__get-trinity-api          // C++ API with game context
mcp__trinitycore__get-spell-info           // Spell data (ID, effects)
mcp__trinitycore__get-creature-full-info   // Creature/NPC data
mcp__trinitycore__get-quest-info           // Quest information
mcp__trinitycore__query-dbc                // DBC/DB2 queries
mcp__trinitycore__get-trinity-workflow     // Complete workflows
mcp__trinitycore__get-combat-rating        // Combat calculations
mcp__trinitycore__get-character-stats      // Character stats by level
```

**Strengths:**
- ✅ Provides game mechanics context that C++ code alone cannot
- ✅ Cross-references with game data (spells, creatures, items)
- ✅ Includes WoW-specific best practices
- ✅ Faster for "what exists?" questions

**Limitations:**
- ❌ May not reflect latest C++ implementation changes
- ❌ Cannot show actual code implementations
- ❌ Cannot find real usage patterns in codebase

---

### 💻 Serena (C++ Implementation)

**What It Provides:**
- Actual TrinityCore C++ source code
- Method implementations with full logic
- Real usage examples from codebase
- Code patterns and architecture
- Always up-to-date with current codebase
- Cross-reference capabilities

**Available Tools:**
```typescript
mcp__serena__find_symbol                   // Find classes/methods
mcp__serena__find_referencing_symbols      // Find usage locations
mcp__serena__search_for_pattern            // Search code patterns
mcp__serena__get_symbols_overview          // File structure
mcp__serena__list_dir                      // Navigate directories
mcp__serena__read_memory                   // Read project knowledge
```

**Strengths:**
- ✅ Shows actual current implementation (source of truth)
- ✅ Reveals real usage patterns from TrinityCore core
- ✅ Always synchronized with codebase
- ✅ Can debug specific implementation issues

**Limitations:**
- ❌ Cannot explain WoW game mechanics context
- ❌ Cannot provide spell IDs, creature entries, zone IDs
- ❌ No knowledge of DBC/DB2 data structures
- ❌ Slower for "what methods exist?" exploration

---

## Mandatory Research Workflow

### Step 1: MCP Query (ALWAYS FIRST)

Start with TrinityCore MCP to get game mechanics context.

**Example:**
```typescript
// Research: "How do bots resurrect at graveyards?"

// Query MCP for game knowledge
mcp__trinitycore__get-trinity-workflow "graveyard-resurrection"
// Returns: Workflow steps, involved systems

mcp__trinitycore__get-spell-info 8326
// Returns: Ghost spell data (applied during death)

mcp__trinitycore__get-creature-full-info 6491
// Returns: Spirit Healer NPC data
```

**Analyze MCP Results:**
- What spell IDs are involved? (e.g., 8326 = Ghost)
- What creature entries are needed? (e.g., 6491 = Spirit Healer)
- What's the recommended workflow?
- What are the common pitfalls?
- What DBC/database data is required?

---

### Step 2: Serena Query (ALWAYS SECOND)

Use Serena to understand C++ implementation.

**Example:**
```typescript
// After MCP provides context, dive into C++ code

// Find method implementations
mcp__serena__find_symbol "Player/BuildPlayerRepop" --include_body true
// Returns: C++ code that creates corpse and applies Ghost aura

mcp__serena__find_symbol "Player/RepopAtGraveyard" --include_body true
// Returns: C++ code that teleports to graveyard

// Find real usage examples
mcp__serena__find_referencing_symbols "Player/RepopAtGraveyard"
// Returns: Where this method is actually used in codebase

// Search for patterns
mcp__serena__search_for_pattern "HandleMoveTeleportAck" --path src/modules/Playerbot
// Returns: How bots handle teleport acknowledgment
```

**Analyze Serena Results:**
- How is it actually implemented?
- What are the method signatures?
- What error handling exists?
- How do other systems use this?
- Are there edge cases in the code?

---

### Step 3: Synthesis (MANDATORY)

Combine both perspectives into complete understanding.

**Complete Answer Format:**
```markdown
## [Research Topic]

### 🎮 Game Mechanics (from MCP)
- Spell: Ghost (ID 8326) - Applied when player dies
- Creature: Spirit Healer (Entry 6491) - Found at graveyards
- Workflow: Death → Corpse Creation → Teleport → Resurrection Choice
- Pitfall: Spell.cpp:603 crash if teleport ack called immediately

### 💻 C++ Implementation (from Serena)
- `Player::BuildPlayerRepop()` - Creates corpse, applies Ghost aura (8326)
- `Player::RepopAtGraveyard()` - Finds nearest graveyard, teleports
- `Player::ResurrectPlayer()` - Restores health/mana, removes Ghost
- Pattern: DeathRecoveryManager handles full workflow

### ⚠️ Critical Implementation Details
- Must defer HandleMoveTeleportAck() by 100ms (prevents spell mod corruption)
- Ghost aura (8326) managed automatically by TrinityCore (don't manage manually)
- Spirit Healer resurrection applies Resurrection Sickness (spell 15007) if level > 10

### ✅ Complete Implementation Example
[Code example combining MCP game knowledge + Serena implementation]
```

---

## Trigger Patterns (When to Use Both)

**Automatically use BOTH tools when you encounter:**

### Research Questions
- "How does [X] work?"
- "Research [X]"
- "Understand [X]"
- "Explain [X]"
- "What is [X]?"

### Implementation Questions
- "Implement [feature]"
- "How do I make bots [action]?"
- "Add [functionality]"
- "Create [system]"

### Game Mechanic Mentions
- Any spell name or ID (e.g., "Ghost", "8326")
- Any creature name or entry (e.g., "Spirit Healer", "6491")
- Any zone name or ID
- Any quest name or ID
- Combat mechanics (threat, damage, healing)
- Movement mechanics (teleport, chase, follow)

### TrinityCore Class Names
- Player, Creature, Unit, Object
- Spell, SpellInfo, Aura, AuraEffect
- Group, Guild, Quest, Map
- MotionMaster, ThreatManager
- WorldSession, CharacterDatabase

---

## Anti-Patterns (FORBIDDEN)

### ❌ Using Only MCP

**Bad Example:**
```
User: "How does bot resurrection work?"
Response: [Only checks MCP, provides workflow but no C++ code]
```

**Why It's Wrong:**
- Missing actual implementation details
- Cannot verify if workflow matches current codebase
- No real usage examples
- Cannot debug implementation issues

---

### ❌ Using Only Serena

**Bad Example:**
```
User: "What spell makes bots ghosts?"
Response: [Only checks C++, finds method but misses spell ID 8326]
```

**Why It's Wrong:**
- Missing game mechanics context
- No spell ID information
- No DBC/database cross-references
- No best practices or common pitfalls

---

### ✅ Correct Approach

**Good Example:**
```
User: "How does bot resurrection work?"

Response:
1. MCP Query:
   - mcp__trinitycore__get-trinity-workflow "graveyard-resurrection"
   - mcp__trinitycore__get-spell-info 8326
   - mcp__trinitycore__get-creature-full-info 6491

2. Serena Query:
   - mcp__serena__find_symbol "Player/BuildPlayerRepop" --include_body true
   - mcp__serena__find_symbol "Player/RepopAtGraveyard" --include_body true
   - mcp__serena__find_referencing_symbols "Player/ResurrectPlayer"

3. Synthesis:
   [Complete answer with game mechanics + C++ implementation + examples]
```

---

## Quality Standards

### Every Research Answer MUST Include:

1. ✅ **Game Mechanics Context (MCP)**
   - Spell IDs, creature entries, zone IDs
   - Workflow steps
   - Common pitfalls

2. ✅ **C++ Implementation (Serena)**
   - Method signatures
   - Actual code implementations
   - Real usage patterns

3. ✅ **Cross-References**
   - DBC/DB2 data (from MCP)
   - Database tables (from MCP)
   - Code locations (from Serena)

4. ✅ **Best Practices**
   - Recommended patterns (from MCP)
   - Actual TrinityCore patterns (from Serena)

5. ✅ **Complete Examples**
   - Working code combining both perspectives

6. ✅ **Error Handling**
   - Common crashes and fixes (from MCP)
   - Edge cases in code (from Serena)

---

## Enforcement

### Incomplete Research Detection

If you provide a research answer using only ONE tool, you MUST self-correct:

```
⚠️ INCOMPLETE RESEARCH DETECTED

This answer only used [MCP/Serena] but requires BOTH tools.

Corrective Action:
1. Query the missing tool
2. Analyze the additional results
3. Provide complete synthesis

Re-researching with both tools...
```

### Quality Checklist

Before providing any research answer, verify:

- [ ] Used MCP for game mechanics context
- [ ] Used Serena for C++ implementation
- [ ] Included spell IDs / creature entries / zone IDs
- [ ] Included method signatures and code
- [ ] Included real usage examples
- [ ] Included best practices and pitfalls
- [ ] Provided complete working code example

**If ANY checkbox is unchecked, the research is INCOMPLETE.**

---

## Practical Examples

### Example 1: Simple API Research

**Question:** "What method do I use to teleport a bot?"

**Research Process:**
```typescript
// Step 1: MCP - Get game context
mcp__trinitycore__get-trinity-api Player --category movement
// Returns: High-level API overview with TeleportTo method

// Step 2: Serena - Get implementation
mcp__serena__find_symbol "Player/TeleportTo" --include_body true
// Returns: Actual C++ signature and logic

// Step 3: Serena - Find usage
mcp__serena__find_referencing_symbols "Player/TeleportTo"
// Returns: Real examples from TrinityCore core
```

**Complete Answer:**
```markdown
### 🎮 Teleport API (from MCP)
- Method: Player::TeleportTo()
- Purpose: Teleports player to location (same-map or cross-map)
- Use Cases: Death recovery, summons, portals, hearthstone

### 💻 Implementation (from Serena)
```cpp
bool Player::TeleportTo(uint32 mapid, float x, float y, float z, float orientation)
{
    // Validates map, handles cross-map vs same-map teleport
    // Updates position, handles visibility, notifies client
}
```

### Usage Example (from Serena)
```cpp
// From DeathRecoveryManager.cpp
bot->RepopAtGraveyard();  // Internally calls TeleportTo()
```

### ✅ Complete Bot Implementation
```cpp
// Teleport bot to coordinates
if (bot->TeleportTo(0, -8949.95f, -132.49f, 83.53f, 0.0f))
{
    TC_LOG_INFO("module.playerbot", "Bot {} teleported to Northshire", bot->GetName());
}
```
```

---

### Example 2: Complex Workflow Research

**Question:** "How do I implement bot death recovery?"

**Research Process:**
```typescript
// Step 1: MCP - Get complete workflow
mcp__trinitycore__get-trinity-workflow "bot-death-recovery"
// Returns: Full workflow with spell IDs and creature entries

mcp__trinitycore__get-spell-info 8326  // Ghost
mcp__trinitycore__get-spell-info 15007 // Resurrection Sickness
mcp__trinitycore__get-creature-full-info 6491  // Spirit Healer

// Step 2: Serena - Get implementation
mcp__serena__find_symbol "Player/BuildPlayerRepop" --include_body true
mcp__serena__find_symbol "Player/RepopAtGraveyard" --include_body true
mcp__serena__search_for_pattern "DeathRecovery" --path src/modules/Playerbot

// Step 3: Serena - Find existing implementation
mcp__serena__find_symbol "DeathRecoveryManager" --depth 1
// Shows all methods in existing death recovery system
```

**Complete Answer:**
[Complex multi-section answer with workflow, spells, creatures, C++ code, state machine, examples]

---

## Summary

**Golden Rule:**
**MCP provides the "WHAT" and "WHY" (game mechanics)**
**Serena provides the "HOW" (C++ implementation)**
**BOTH are required for complete understanding**

**Never use only one tool for TrinityCore research. Period.**

---

**This memory is PERMANENT and MANDATORY for all research tasks.**
