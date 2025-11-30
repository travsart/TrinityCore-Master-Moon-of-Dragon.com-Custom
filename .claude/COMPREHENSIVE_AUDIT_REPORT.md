# Comprehensive Playerbot Codebase Audit Report

**Date:** November 27, 2025
**Scope:** Full audit of `src/modules/Playerbot/` for incomplete implementations
**Status:** AUDIT COMPLETE

---

## Executive Summary

This audit identifies all TODO comments, stub implementations, placeholders, simplified implementations, workarounds, and NO-OP patterns in the Playerbot codebase. Each issue is classified by severity and recommended action.

### Statistics Overview

| Category | Count | Priority |
|----------|-------|----------|
| TODO Comments (Playerbot .cpp) | ~77 | Mixed |
| TODO Comments (Playerbot .h) | ~4 | Low |
| Stub Implementations | 8 | HIGH |
| Placeholder Values | 9 | MEDIUM |
| Simplified Implementations | 5 | MEDIUM |
| Workarounds | 3 | LOW |
| NO-OP Methods (Intentional) | 2 | INFO |
| Obsolete/Outdated TODOs | 32 | HIGH - Remove |

---

## CRITICAL ISSUES (Priority: HIGH)

### 1. InteractionManager.cpp - 32 Obsolete TODOs

**File:** `src/modules/Playerbot/Interaction/InteractionManager.cpp`
**Issue:** Contains 32 TODO comments referencing files that "need to be created" but ALREADY EXIST as separate implementations.

**Affected Lines:** 15-19, 110-114, 146-152, 526-582

**Examples:**
```cpp
// #include "../Trainers/TrainerInteraction.h"  // TODO: Create this file
// #include "../Vendors/VendorInteraction.h"    // TODO: Create this file
```

**Reality:** These handlers exist:
- `TrainerInteractionManager.cpp` - EXISTS
- `VendorInteractionManager.cpp` - EXISTS
- `InnkeeperInteractionManager.cpp` - EXISTS
- `FlightMasterInteractionManager.cpp` - EXISTS
- `BankInteractionManager.cpp` - EXISTS
- `MailboxInteractionManager.cpp` - EXISTS
- `AuctioneerInteractionManager.cpp` - EXISTS
- `RepairInteractionManager.cpp` - EXISTS
- `StableMasterInteractionManager.cpp` - EXISTS
- `SpiritHealerInteractionManager.cpp` - EXISTS

**Action Required:** REMOVE all 32 obsolete TODO comments. The InteractionManager is a facade - the separate handler files are the complete implementations.

---

### 2. Header Files Documenting "Stub Implementation" Problem

**Files with "Problem: Stub implementation with NO-OP methods" in docstrings:**

| File | Line | Status |
|------|------|--------|
| `AI/Combat/TargetManager.h` | 81 | Needs verification |
| `AI/Combat/CrowdControlManager.h` | 76 | Needs verification |
| `AI/Combat/DefensiveManager.h` | 84 | Needs verification |
| `AI/Combat/MovementIntegration.h` | 114 | Needs verification |

**Action Required:** Verify if implementations in corresponding .cpp files are complete. If complete, update header docstrings to remove "Problem: Stub" language.

---

### 3. BaselineRotationManager.cpp - Documented Stubs

**File:** `src/modules/Playerbot/AI/ClassAI/BaselineRotationManager.cpp`

**Issues:**
- Line 586: `// Additional Classes (stub implementations for brevity)`
- Line 669: `// Stub implementations for other baseline rotations`

**Action Required:** Implement complete rotations for all classes or document which classes are intentionally simplified.

---

### 4. TargetAssistAction.cpp - API Compatibility Stub

**File:** `src/modules/Playerbot/AI/Actions/TargetAssistAction.cpp`
**Line:** 25

```cpp
// This implementation is a stub due to TrinityCore API compatibility issues
```

**Action Required:** Research TrinityCore 11.2 API and implement proper target assist functionality.

---

### 5. MountManager.cpp - DBC/DB2 Stub

**File:** `src/modules/Playerbot/AI/Mobility/MountManager.cpp`
**Line:** 773

```cpp
// This is a stub implementation - full mount database would load from DBC/DB2
```

**Action Required:** Implement proper mount database loading from DBC/DB2 data.

---

## MEDIUM PRIORITY ISSUES

### 6. SoloStrategy.cpp - 11 Missing Manager Methods

**File:** `src/modules/Playerbot/AI/Strategy/SoloStrategy.cpp`

**Missing Methods:**
| Manager | Missing Method | Line |
|---------|----------------|------|
| QuestManager | HasActiveQuests() | ~150 |
| QuestManager | ProgressQuests() | ~155 |
| GatheringManager | HasGatheringProfession() | ~180 |
| GatheringManager | GatherNearestResource() | ~185 |
| TradeManager | HasItemsToSell() | ~210 |
| TradeManager | HasItemsToBuy() | ~215 |
| TradeManager | ProcessPendingTrades() | ~220 |
| AuctionManager | HasPendingAuctions() | ~245 |
| AuctionManager | HasExpiredAuctions() | ~250 |
| AuctionManager | ProcessAuctions() | ~255 |
| QuestManager | HasAvailableQuests() | ~280 |
| QuestManager | HasCompletedQuests() | ~285 |

**Line ~310:** `// TODO: Implement proper wandering with pathfinding`

**Action Required:** Add these helper methods to respective managers or implement inline.

---

### 7. BotStatePersistence.cpp - 6 Placeholder Comments

**File:** `src/modules/Playerbot/Core/BotStatePersistence.cpp`

| Line | Comment |
|------|---------|
| 93 | `// Placeholder: Mark as async pending` |
| 155 | `// Placeholder: Return not found for demonstration` |
| 268 | `// Placeholder implementation` |
| 378 | `// Placeholder implementation` |
| 675 | `// Placeholder database query` |
| 700 | `// Placeholder persistence` |

**Action Required:** Replace placeholder implementations with actual database operations.

---

### 8. BehaviorTreeFactory.cpp - Placeholder Spell IDs

**File:** `src/modules/Playerbot/AI/BehaviorTree/BehaviorTreeFactory.cpp`

| Line | Issue |
|------|-------|
| 1252 | `// Cast AoE heal (placeholder)` |
| 1281 | `// Placeholder spell ID` |

**Action Required:** Replace placeholder spell IDs with actual class-specific spell IDs from DBC data.

---

### 9. Hunter Class PetInfo Stubs

**Files:**
- `AI/ClassAI/Hunters/SurvivalHunter.h:916` - PetInfo stub
- `AI/ClassAI/Hunters/BeastMasteryHunter.h:703` - PetInfo stub

**Action Required:** Implement proper pet information retrieval from TrinityCore pet system.

---

## LOW PRIORITY ISSUES

### 10. BotAccountMgr.cpp - Simplified Implementation

**File:** `src/modules/Playerbot/Core/BotAccountMgr.cpp`

| Line | Comment |
|------|---------|
| 610 | `// For our simplified implementation...` |
| 852 | `info.createdAt = ::std::chrono::system_clock::now(); // Placeholder` |

**Status:** May be intentional simplification for bot accounts (not real player accounts).

---

### 11. ActionPriority.cpp - TBB Workaround

**File:** `src/modules/Playerbot/AI/Actions/ActionPriority.cpp`
**Line:** 70

```cpp
// TBB doesn't have try_peek, so we'll implement a workaround
```

**Status:** Legitimate workaround for library limitation. Document but don't change.

---

### 12. MySQLWorkaround.h References

**Files referencing workaround:**
- Various database files include `MySQLWorkaround.h`

**Status:** Legitimate workaround for MySQL compatibility. Document but don't change.

---

### 13. FlightMasterManager.cpp - Not Implemented Return

**File:** `src/modules/Playerbot/Interaction/FlightMasterManager.cpp`
**Line:** 494

```cpp
// For now, return 0 (not implemented)
```

**Action Required:** Implement proper flight cost calculation.

---

### 14. AuctionManager.cpp - Price History Not Implemented

**File:** `src/modules/Playerbot/Economy/AuctionManager.cpp`
**Line:** 869

```cpp
// Price history loading skipped (not implemented)
```

**Action Required:** Implement price history tracking if needed for market analysis.

---

## INFORMATIONAL (No Action Required)

### 15. ObjectCache.h - Intentional NO-OP

**File:** `src/modules/Playerbot/Core/ObjectCache.h`
**Line:** 100

```cpp
RefreshCache() { /* NO-OP - cache is populated via Set* methods */ }
```

**Status:** Intentional design - cache refresh happens via setters, not explicit refresh.

---

### 16. ProfessionManager.cpp - Intentional NO-OP

**File:** `src/modules/Playerbot/Professions/ProfessionManager.cpp`
**Line:** 61

```cpp
// INITIALIZATION (NOW NO-OP)
```

**Status:** Intentional - lazy initialization pattern.

---

### 17. EventBus Template Filtering

**Files:** Various EventBus files

```cpp
// Unit-specific filtering not implemented in template
```

**Status:** Design limitation of template system - acceptable.

---

### 18. Hunter Pet Feeding - WoW 11.2 Design

**Comment:** `// not implemented in WoW 11.2`

**Status:** Correct - WoW 11.2 removed hunter pet happiness/feeding system. This is accurate documentation.

---

## BotAI_EventHandlers.cpp - 9 TODOs

**File:** `src/modules/Playerbot/AI/BotAI_EventHandlers.cpp`

| Line | TODO Description |
|------|------------------|
| ~45 | Event handler completion |
| ~78 | Combat event handling |
| ~112 | Death event handling |
| ~145 | Resurrection handling |
| ~178 | Group event handling |
| ~210 | Quest event handling |
| ~245 | Loot event handling |
| ~278 | Trade event handling |
| ~310 | Chat event handling |

**Action Required:** Review each TODO and implement missing event handlers.

---

## RaidOrchestrator.cpp - 4 TODOs

**File:** `src/modules/Playerbot/Group/RaidOrchestrator.cpp`

**TODOs:** Primarily redesign comments for GroupCoordinator integration.

**Status:** May be obsolete if GroupCoordinator was implemented.

---

## LFGBotManager.cpp - 3 TODOs

**File:** `src/modules/Playerbot/LFG/LFGBotManager.cpp`

| Topic | Description |
|-------|-------------|
| Config loading | Load LFG configuration from playerbots.conf |
| Raid composition | Implement proper raid role distribution |
| Queue management | Implement LFG queue timing logic |

---

## Summary Action Items

### Immediate Actions (HIGH Priority)
1. Remove 32 obsolete TODOs from InteractionManager.cpp
2. Verify 4 combat header implementations and update docstrings
3. Complete BaselineRotationManager stub rotations
4. Fix TargetAssistAction TrinityCore API compatibility
5. Implement MountManager DBC/DB2 loading

### Short-term Actions (MEDIUM Priority)
6. Add 11 missing manager methods for SoloStrategy
7. Replace 6 BotStatePersistence placeholders
8. Fix BehaviorTreeFactory placeholder spell IDs
9. Implement Hunter PetInfo properly

### Documentation Updates (LOW Priority)
10. Document intentional workarounds (TBB, MySQL)
11. Mark intentional NO-OPs in code comments
12. Update INCOMPLETE_IMPLEMENTATIONS_REPORT.md

---

## Files Modified Since Last Audit

The following files have been modified and staged:
- 66 files committed for cleanup (Python scripts + backup removed)
- Build verified successful (worldserver.exe: 55MB)

---

## Conclusion

The Playerbot codebase has **significant technical debt** in the form of:
- 32 obsolete TODO comments in InteractionManager.cpp
- 8 documented stub implementations
- 9 placeholder values
- 11 missing manager helper methods

**Estimated cleanup effort:** 2-3 days of focused development work.

**Priority recommendation:** Start with InteractionManager.cpp cleanup (removes 32 TODOs immediately), then verify combat header implementations.
