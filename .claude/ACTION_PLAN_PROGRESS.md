# Action Plan Execution Progress

**Started**: 2026-01-23
**Status**: IN PROGRESS
**Current Phase**: Phase 2 - Short-Term Improvements (STARTING)

---

## Phase 1: Quick Wins (0-2 weeks)
**Target**: 25-40% CPU reduction, 5-10% memory reduction
**Actual Results**:
- ✅ **3/6 items COMPLETED** (QW-2, QW-3, QW-4)
- 🔬 **2/6 items ANALYZED** - deferred to Phase 2 with detailed findings (QW-1, QW-6)
- 📋 **1/6 item DEFERRED** - requires new implementation (QW-5)

**Estimated Impact (3 completed items)**:
- Memory leak eliminated (QW-4: static objects → instance variables)
- ~15-25% target selection improvement (QW-2: O(n²) → O(n) caching)
- Reduced memory reallocations in hot paths (QW-3: reserve() calls)

### QW-4: Fix ClassAI Static Object Bug
- **Status**: ✅ COMPLETED
- **Priority**: P0 (CRITICAL - Memory Leak)
- **Started**: 2026-01-23
- **Completed**: 2026-01-23
- **Commit**: ec2095f443
- **Notes**: Fixed 7 static variable bugs in ClassAI.cpp, CombatSpecializationBase.cpp, DemonHunterAI.cpp, EvokerAI.cpp, MageAI.cpp, PriestAI.cpp. All converted to per-instance member variables.

### QW-2: Optimize Target Selection Algorithm (O(n²) → O(n))
- **Status**: ✅ COMPLETED
- **Priority**: P0 (CRITICAL)
- **Started**: 2026-01-23
- **Completed**: 2026-01-23
- **Commit**: fda478532b
- **Notes**: Implemented threat score caching (500ms refresh) and group focus caching to eliminate redundant GetThreat() calls and O(n*m) group member iteration. Added _threatScoreCache and _groupFocusCache with RefreshThreatScoreCache(), RefreshGroupFocusCache(), GetCachedThreatScore(), GetCachedGroupFocusCount() methods.

### QW-3: Add Container reserve() Calls to Hot Paths
- **Status**: ✅ COMPLETED
- **Priority**: P1 (HIGH)
- **Started**: 2026-01-23
- **Completed**: 2026-01-23
- **Commit**: 83a570b165
- **Notes**: Added reserve() calls to 7 hot path functions:
  - TargetSelector::GetNearbyEnemies() - reserve(hostileSnapshots.size())
  - TargetSelector::GetNearbyAllies() - reserve(group->GetMembersCount() + 5)
  - BotThreatManager::GetAllThreatTargets() - reserve(_threatMap.Size())
  - BotThreatManager::GetThreatTargetsByPriority() - reserve(_threatMap.Size() / 3)
  - CrowdControlManager::GetAvailableCCSpells() - reserve(classIt->second.size())
  - PositionManager::GenerateCandidatePositions() - reserve(24) for RANGED_DPS
  - TargetManager::GetCombatTargets() - reserve(threatMgr.GetThreatListSize())

### QW-5: Implement Spatial Query Caching
- **Status**: 📋 DEFERRED TO PHASE 2
- **Priority**: P1 (HIGH)
- **Started**: 2026-01-23
- **Completed**: -
- **Commit**: -
- **Findings**: SpatialHostileCache.h header exists with sophisticated design (RCU pattern, zone partitioning, worker threads, per-bot LRU cache) but SpatialHostileCache.cpp is NOT IMPLEMENTED. Current code uses SpatialGridQueryHelpers directly without caching. Implementation requires:
  - CellCache with RCU atomic pointer swapping
  - ZoneCache with 16x16 spatial grid
  - Worker thread for background updates
  - BotLocalHostileCache with LRU eviction
  - This is beyond "Quick Win" scope - deferred to Phase 2

### QW-1: Fix Recursive Mutex Performance Degradation
- **Status**: 🔬 ANALYZED - DEFERRED TO PHASE 2 (Safe to change but needs regression testing)
- **Priority**: P0 (CRITICAL) → P1 (Phase 2)
- **Started**: 2026-01-23
- **Completed**: Analysis Complete
- **Commit**: -
- **Extensive Analysis Results**:

  **Files Analyzed:**
  - BotAI.h/cpp - 9 lock_guard calls at lines 844, 1372, 1480, 1556, 1621, 1640, 1654, 1677, 1723
  - LockHierarchy.h - OrderedRecursiveMutex wrapper with lock order enforcement
  - Strategy.h/cpp - Base IsActive() implementation
  - 7 derived Strategy classes: SoloStrategy, GrindStrategy, LootStrategy, GroupCombatStrategy, SoloCombatStrategy, RestStrategy, QuestStrategy
  - TacticalCoordinator.h/cpp - 26 lock_guard calls

  **Key Findings:**

  1. **Historical Context (BotAI.h:995-1010):**
     - Comment documents "DEADLOCK FIX #14" after "13 failed fixes"
     - std::shared_mutex was replaced with std::recursive_mutex
     - Reason: "complex callback chain (strategies -> triggers -> actions -> group handlers -> back to strategies)"

  2. **Current Code Refactoring Pattern:**
     - All Strategy callbacks (OnActivate, OnDeactivate) called AFTER lock release
     - Example at line 1704: `} // RELEASE LOCK BEFORE CALLBACK`
     - Extensive "DEADLOCK FIX #N" comments throughout codebase

  3. **Strategy::IsActive() Analysis:**
     - Base class (line 66): `return _active;` - simple atomic read, no BotAI calls
     - All 7 overrides only call:
       - `ai->GetBot()` - direct pointer access (no lock)
       - TrinityCore Player methods (IsInCombat, GetGroup, IsInWorld)
       - GroupCombatStrategy::IsGroupInCombat() - iterates group, no BotAI mutex calls
     - **CONCLUSION: No IsActive() implementation calls BotAI mutex-protected methods**

  4. **Inconsistency Found:**
     - Lines 1399, 1421, 1686: `IsActive(this)` called WHILE holding lock
     - Line 864: `IsActive(this)` called AFTER lock release
     - This inconsistency is safe because IsActive() doesn't re-enter the mutex

  **Risk Assessment:**
  - **Theoretical Gain**: 35-50% throughput improvement (recursive_mutex is 2-3x slower than mutex)
  - **Actual Impact**: Depends on lock contention frequency (needs profiling)
  - **Risk Level**: MEDIUM - Analysis suggests recursion doesn't occur, but extensive testing required
  - **Complexity**: The callback refactoring pattern may have edge cases not visible in static analysis

  **Recommendation:**
  - **DO NOT replace in Phase 1** - Risk of reintroducing subtle deadlocks
  - Move to **Phase 2 (ST-2)** with dedicated regression testing phase
  - Add profiling instrumentation to measure actual lock contention
  - Consider gradual rollout: replace one mutex at a time with extensive testing

### QW-6: Fix Naming Inconsistencies (Manager vs Mgr)
- **Status**: 🔬 ANALYZED - DEFERRED (Blocked by ST-4/ST-5/ST-6 consolidations)
- **Priority**: P2 (MEDIUM) → Phase 2/3 dependency
- **Started**: 2026-01-23
- **Completed**: Analysis Complete
- **Commit**: -
- **Naming Analysis Results**:

  **"Mgr" suffix files (14 total):**
  - Account: BotAccountMgr.h, IBotAccountMgr.h
  - Lifecycle: BotLifecycleMgr.h, IBotLifecycleMgr.h
  - Names: BotNameMgr.h, IBotNameMgr.h
  - Sessions: BotWorldSessionMgr.h, BotWorldSessionMgrOptimized.h, IBotWorldSessionMgr.h
  - Database: PlayerbotMigrationMgr.h, IPlayerbotMigrationMgr.h
  - Dungeon: DungeonScriptMgr.h, IDungeonScriptMgr.h
  - Travel: DragonridingMgr.h

  **"Manager" suffix files (90+ files):**
  - Standard naming convention across most codebase

  **Critical Duplication Identified:**
  - `BotLifecycleMgr` (IBotLifecycleMgr) ≠ `BotLifecycleManager` (IBotLifecycleManager) - TWO SEPARATE IMPLEMENTATIONS
  - This is already captured in ST-4: "Consolidate BotLifecycleManager ⊕ BotLifecycleMgr"

  **Dependency Chain:**
  - QW-6 cannot be completed until ST-4, ST-5, ST-6 consolidations are done
  - Renaming before consolidation would create wasted effort
  - After consolidation: Simple rename from "Mgr" → "Manager" across remaining files

  **Recommendation:**
  - Defer QW-6 until Phase 2 consolidation tasks complete
  - Then execute batch rename operation for remaining inconsistent names
  - Estimated effort: ~2 hours after consolidations complete

---

## Phase 2: Short-Term Improvements (1-3 months)
**Target**: 10-15% additional CPU reduction

### ST-1: Implement Adaptive AI Update Throttling
- **Status**: ✅ COMPLETED
- **Priority**: P0 (CRITICAL)
- **Started**: 2026-01-23
- **Completed**: 2026-01-23
- **Commit**: (pending)
- **Implementation Details**:

  **New Files Created:**
  - `AI/AdaptiveAIUpdateThrottler.h` - Header with throttle tier definitions and metrics
  - `AI/AdaptiveAIUpdateThrottler.cpp` - Implementation with proximity detection and activity classification

  **Core Integration:**
  - Integrated into BotAI constructor (creates throttler instance)
  - Integrated into BotAI::UpdateAI() with `ShouldUpdate()` check
  - Integrated into OnCombatStart()/OnCombatEnd() for combat state notifications

  **Throttle Tiers:**
  - `FULL_RATE (100%)`: Near humans (<100m), in combat, or group with human leader
  - `HIGH_RATE (75%)`: Moderate distance (100-250m), active questing/grinding
  - `MEDIUM_RATE (50%)`: Far from players (250-500m), simple following
  - `LOW_RATE (25%)`: Very far (500-1000m), minimal activity
  - `MINIMAL_RATE (10%)`: Idle, out of range (>1000m), no tasks

  **Key Features:**
  - Proximity-based throttling (checks every 2 seconds for nearby human players)
  - Combat state override (combat = FULL_RATE always)
  - Activity classification (COMBAT, QUESTING, FOLLOWING, TRAVELING, IDLE, etc.)
  - Group leader detection (human leader = FULL_RATE for group bots)
  - Global statistics singleton for monitoring overall throttle effectiveness
  - ForceNextUpdate() method for critical events

  **Expected Impact:**
  - 10-15% CPU reduction for bots far from human players
  - Zero impact on bots near players (full responsiveness maintained)
  - Zero impact on bots in combat (always full update rate)

### ST-2: Fix Packet Queue Lock Contention
- **Status**: ⏳ PENDING
- **Priority**: P1 (HIGH)

### ST-3: Expand Object Pooling to Hot Paths
- **Status**: ⏳ PENDING
- **Priority**: P1 (HIGH)

### ST-4: Consolidate BotLifecycleManager ⊕ BotLifecycleMgr
- **Status**: 🔬 ANALYZED - NOT A DUPLICATION (Naming Confusion Only)
- **Priority**: P0 → P3 (LOW - Rename only)
- **Started**: 2026-01-23
- **Completed**: Analysis Complete
- **Analysis Results**:

  **BotLifecycleManager** (per-bot controller):
  - Constructor: `BotLifecycleManager(Player* bot)` - takes individual bot
  - Purpose: Individual bot state management (ACTIVE, IDLE, COMBAT, QUESTING)
  - Methods: CreateBotLifecycle(), GetBotLifecycle(), GetActiveLifecycles()
  - Implements: IBotLifecycleManager interface

  **BotLifecycleMgr** (system singleton coordinator):
  - Constructor: `static BotLifecycleMgr* instance()` - singleton
  - Purpose: Global bot population coordination, zone balancing
  - Methods: Initialize(), ProcessSchedulerEvents(), OnBotSpawnSuccess()
  - Implements: IBotLifecycleMgr interface
  - Has worker thread for background processing

  **CONCLUSION: NOT a duplication issue**
  - Different responsibility levels (micro vs macro)
  - Different interfaces (per-bot vs system-wide)
  - Naming is confusing but functionality is distinct

  **Recommendation:**
  - Rename `BotLifecycleMgr` → `BotPopulationCoordinator` (clarifies system-level role)
  - Keep `BotLifecycleManager` as-is (per-bot lifecycle is clear)
  - This is a naming improvement, not a consolidation

### ST-5: Consolidate QuestManager → UnifiedQuestManager
- **Status**: ⏳ PENDING
- **Priority**: P1 (HIGH)

### ST-6: Consolidate TargetSelector ⊕ TargetManager ⊕ TargetScanner
- **Status**: ⏳ PENDING
- **Priority**: P2 (MEDIUM)

### ST-7: Add Comprehensive Exception Handling
- **Status**: ⏳ PENDING
- **Priority**: P2 (MEDIUM)

### ST-8: Optimize Include Hierarchy
- **Status**: ⏳ PENDING
- **Priority**: P2 (MEDIUM)

---

## Phase 3: Medium-Term Refactoring (3-6 months)
**Target**: 5-10% additional CPU reduction

### MT-1: Consolidate Interrupt Systems (4x Redundancy)
- **Status**: ⏳ PENDING

### MT-2: Consolidate FormationManager ⊕ RoleBasedCombatPositioning
- **Status**: ⏳ PENDING

### MT-3: Consolidate ThreatCoordinator ⊕ BotThreatManager
- **Status**: ⏳ PENDING

### MT-4: Consolidate LootManager ⊕ UnifiedLootManager
- **Status**: ⏳ PENDING

### MT-5: Consolidate Remaining 3 Medium-Priority Managers
- **Status**: ⏳ PENDING

### MT-6: Standardize Lock-Free Patterns
- **Status**: ⏳ PENDING

### MT-7: Implement Unified Caching Layer
- **Status**: ⏳ PENDING

### MT-8: Refactor Large Files (>50KB)
- **Status**: ⏳ PENDING

---

## Phase 4: Long-Term Architecture Evolution (6-12 months)
**Target**: 15-25% additional CPU reduction

### LT-1: Implement TBB Parallel Bot Updates
- **Status**: ⏳ PENDING

### LT-2: Parallelize Combat Calculations
- **Status**: ⏳ PENDING

### LT-3: Parallelize Pathfinding
- **Status**: ⏳ PENDING

### LT-4: Expand Event-Driven AI Architecture
- **Status**: ⏳ PENDING

### LT-5: Modular Combat System
- **Status**: ⏳ PENDING

### LT-6: Migration to Modern C++20/23 Patterns
- **Status**: ⏳ PENDING

---

## Metrics Tracking

| Metric | Baseline | After Phase 1 | After Phase 2 | After Phase 3 | After Phase 4 |
|--------|----------|---------------|---------------|---------------|---------------|
| CPU (1000 bots) | TBD | - | - | - | - |
| Memory (1000 bots) | TBD | - | - | - | - |
| Target Selection Time | TBD | - | - | - | - |
| Manager Count | TBD | - | - | - | - |
| Total LOC | TBD | - | - | - | - |

---

## Commit History

| Date | Item | Commit Hash | Description |
|------|------|-------------|-------------|
| 2026-01-23 | QW-4 | ec2095f443 | Fix 7 static variable memory leaks in ClassAI files |
| 2026-01-23 | QW-2 | fda478532b | Implement threat/group focus caching (O(n²) → O(n)) |
| 2026-01-23 | QW-3 | 83a570b165 | Add reserve() calls to 7 hot path vector allocations |

