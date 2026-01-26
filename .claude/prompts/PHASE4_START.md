# Phase 4: New Coordinators - Claude Code Start Prompt

Copy this entire prompt into Claude Code to begin Phase 4 implementation:

---

## PROMPT START

Phases 1, 2, and 3 are complete. Now implement Phase 4: New Coordinators.

### Context
- **Repository**: `C:\TrinityBots\TrinityCore`
- **Detailed Plan**: Read `.claude/prompts/PHASE4_NEW_COORDINATORS.md` for full specifications
- **Master Plan**: Read `.claude/plans/COMBAT_REFACTORING_MASTER_PLAN.md`

### Phase 4 Goal
Implement the three missing coordinators identified in the analysis:
- **DungeonCoordinator** - 5-man dungeon coordination (trash, bosses, M+, wipe recovery)
- **ArenaCoordinator** - PvP coordination (kill targets, burst windows, CC chains)
- **BattlegroundCoordinator** - Large-scale PvP (objectives, roles, strategic decisions)

---

### Phase 4 Tasks (Execute in Order)

---

## Task 4.1: DungeonCoordinator (80h / 2 weeks)

**Files to CREATE** in `src/modules/Playerbot/AI/Coordination/Dungeon/`:

### 4.1.1: DungeonState.h
```cpp
enum class DungeonState : uint8 {
    IDLE, ENTERING, READY_CHECK, CLEARING_TRASH, 
    PRE_BOSS, BOSS_COMBAT, POST_BOSS, WIPED, COMPLETED
};

enum class DungeonDifficulty : uint8 { NORMAL, HEROIC, MYTHIC, MYTHIC_PLUS };
enum class TrashPackPriority : uint8 { SKIP, OPTIONAL, REQUIRED, PATROL, DANGEROUS };
enum class RaidMarker : uint8 { NONE, SKULL, CROSS, MOON, SQUARE, TRIANGLE, DIAMOND, CIRCLE, STAR };

struct TrashPack { uint32 packId; std::vector<ObjectGuid> members; TrashPackPriority priority; bool requiresCC; };
struct BossInfo { uint32 bossId; uint8 currentPhase; bool hasEnrageTimer; uint32 enrageTimeMs; };
struct DungeonProgress { uint32 dungeonId; uint8 bossesKilled; float completionPercent; /* M+ fields */ };
```

### 4.1.2: DungeonCoordinator.h/.cpp
- State machine (IDLE → ENTERING → CLEARING_TRASH → PRE_BOSS → BOSS_COMBAT → ...)
- Inherits `ICombatEventSubscriber`
- Manages: TrashPullManager, BossEncounterManager, WipeRecoveryManager, MythicPlusManager
- Key methods: `GetCurrentPullTarget()`, `AssignCCTargets()`, `OnBossEngaged()`, `OnGroupWipe()`

### 4.1.3: TrashPullManager.h/.cpp
- Pull planning with CC assignments
- Raid marker management (Skull = kill first, Moon = poly, etc.)
- Kill order calculation
- Safe pull detection

### 4.1.4: BossEncounterManager.h/.cpp
- Boss strategies with phase tracking
- Mechanic detection and handling (tank swap, spread, stack, interrupt)
- Enrage timer tracking
- Bloodlust timing

### 4.1.5: WipeRecoveryManager.h/.cpp
- Rez priority queue (Healer > Tank > DPS)
- Rebuff coordination
- Mana regeneration waiting
- Ready check before continuing

### 4.1.6: MythicPlusManager.h/.cpp
- Timer tracking (remaining time, 2/3 chest thresholds)
- Affix handling (Bolstering, Raging, Sanguine, Explosive, etc.)
- Enemy forces tracking (% completion)
- Route optimization

---

## Task 4.2: ArenaCoordinator (80h / 2 weeks)

**Files to CREATE** in `src/modules/Playerbot/AI/Coordination/Arena/`:

### 4.2.1: ArenaState.h
```cpp
enum class ArenaState : uint8 { IDLE, QUEUED, PREPARATION, GATES_OPENING, COMBAT, VICTORY, DEFEAT };
enum class ArenaType : uint8 { ARENA_2V2 = 2, ARENA_3V3 = 3, ARENA_5V5 = 5 };
enum class ArenaRole : uint8 { UNKNOWN, HEALER, MELEE_DPS, RANGED_DPS, HYBRID };
enum class TargetPriority : uint8 { LOW, NORMAL, HIGH, KILL_TARGET };

struct ArenaEnemy { ObjectGuid guid; ArenaRole role; bool trinketAvailable; float healthPercent; bool isInCC; };
struct ArenaTeammate { ObjectGuid guid; ArenaRole role; bool needsPeel; bool hasDefensivesAvailable; };
struct BurstWindow { ObjectGuid target; uint32 startTime; uint32 duration; bool isActive; };
```

### 4.2.2: ArenaCoordinator.h/.cpp
- State machine (PREPARATION → GATES_OPENING → COMBAT → VICTORY/DEFEAT)
- Manages: KillTargetManager, BurstCoordinator, CCChainManager, DefensiveCoordinator
- Enemy cooldown tracking (trinkets, defensives, major CDs)
- Key methods: `GetKillTarget()`, `CallBurst()`, `CallCCChain()`, `RequestPeel()`

### 4.2.3: KillTargetManager.h/.cpp
- Target scoring based on: health, trinket status, defensive status, role, position
- Switch target logic
- Priority bonus system

### 4.2.4: CCChainManager.h/.cpp
- **Uses DR tracking from Phase 2** (CrowdControlManager)
- CC chain planning considering DR stacks
- Chain execution coordination (poly → fear → stun with proper timing)
- Overlap prevention

### 4.2.5: BurstCoordinator.h/.cpp
- Burst window management
- Cooldown stacking coordination
- "Go" signal timing

### 4.2.6: DefensiveCoordinator.h/.cpp
- Peel request management
- Defensive cooldown rotation
- External CD coordination (Pain Suppression, Blessing of Sacrifice, etc.)

---

## Task 4.3: BattlegroundCoordinator (80h / 2 weeks)

**Files to CREATE** in `src/modules/Playerbot/AI/Coordination/Battleground/`:

### 4.3.1: BGState.h
```cpp
enum class BGState : uint8 { IDLE, QUEUED, PREPARATION, ACTIVE, OVERTIME, VICTORY, DEFEAT };
enum class BGType : uint32 { WARSONG_GULCH = 489, ARATHI_BASIN = 529, ALTERAC_VALLEY = 30, ... };
enum class BGRole : uint8 { UNASSIGNED, FLAG_CARRIER, FLAG_ESCORT, FLAG_HUNTER, NODE_ATTACKER, NODE_DEFENDER, ROAMER, ... };
enum class ObjectiveType : uint8 { FLAG, NODE, TOWER, GRAVEYARD, GATE, CART, ORB, BOSS };
enum class ObjectiveState : uint8 { NEUTRAL, ALLIANCE_CONTROLLED, HORDE_CONTROLLED, CONTESTED, ... };

struct BGObjective { uint32 id; ObjectiveType type; ObjectiveState state; float captureProgress; };
struct BGScoreInfo { uint32 allianceScore; uint32 hordeScore; uint32 timeRemaining; };
struct FlagInfo { ObjectGuid carrierGuid; bool isPickedUp; uint8 stackCount; };
```

### 4.3.2: BattlegroundCoordinator.h/.cpp
- Supports 11 BG types (WSG, AB, AV, EOTS, SOTA, IOC, TP, BFG, SSM, TK, DG)
- Manages: ObjectiveManager, BGRoleManager, FlagCarrierManager, NodeController, BGStrategyEngine
- Key methods: `GetBotRole()`, `CommandAttack()`, `CommandDefend()`, `GetFriendlyFC()`

### 4.3.3: BGRoleManager.h/.cpp
- Role assignment based on class/spec suitability
- Dynamic reassignment based on situation
- Role requirements per BG type

### 4.3.4: FlagCarrierManager.h/.cpp (CTF maps)
- FC selection (best survivability)
- Escort coordination
- Enemy FC hunting
- Kiting paths

### 4.3.5: NodeController.h/.cpp (Node maps)
- Capture coordination
- Defender allocation
- Contest decisions

### 4.3.6: BGStrategyEngine.h/.cpp
- Strategic decisions: BALANCED, AGGRESSIVE, DEFENSIVE, TURTLE, ALL_IN, STALL
- Score-based strategy adjustment
- Time-remaining considerations
- Map-specific strategies

---

### Instructions
1. Read the detailed plan first: `.claude/prompts/PHASE4_NEW_COORDINATORS.md`
2. Execute tasks in order (4.1 → 4.2 → 4.3)
3. Each coordinator should be functional before moving to next
4. Use event system from Phase 3 (ICombatEventSubscriber)
5. Use DR tracking from Phase 2 (CrowdControlManager) for Arena CC chains
6. Verify compilation after each major component
7. Commit after each working sub-task

### Implementation Order (6 weeks)
| Week | Task |
|------|------|
| 1 | DungeonCoordinator core + TrashPullManager |
| 2 | BossEncounterManager + WipeRecoveryManager + MythicPlusManager |
| 3 | ArenaCoordinator core + KillTargetManager |
| 4 | CCChainManager + BurstCoordinator + DefensiveCoordinator |
| 5 | BattlegroundCoordinator core + BGRoleManager |
| 6 | FlagCarrierManager + NodeController + BGStrategyEngine + Integration |

### Success Criteria
- [ ] DungeonCoordinator handles trash pulls with CC assignments
- [ ] DungeonCoordinator handles boss encounters with phase tracking
- [ ] DungeonCoordinator handles wipe recovery (rez order, rebuff)
- [ ] MythicPlusManager tracks timer and enemy forces
- [ ] ArenaCoordinator manages kill targets and burst windows
- [ ] CCChainManager plans DR-aware CC chains
- [ ] BattlegroundCoordinator assigns roles per BG type
- [ ] BGStrategyEngine makes strategic decisions based on score/time
- [ ] All coordinators use event system (ICombatEventSubscriber)
- [ ] Project compiles without errors

### Critical Notes
1. **Use existing systems**: Phase 2 DR tracking, Phase 3 event system
2. **State machines**: All coordinators should have proper state management
3. **Factory pattern**: Use factory for BG-specific initialization
4. **Test incrementally**: Each manager should work standalone before integration

Start with Task 4.1.1 (DungeonState.h).

---

## PROMPT END
