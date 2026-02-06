# Full SDD workflow

## Configuration
- **Artifacts Path**: {@artifacts_path} → `.zenflow/tasks/{task_id}`

---

## Workflow Steps

### [x] Step: Requirements
<!-- chat-id: bf944867-8bed-4bd3-b35d-ec4a59183481 -->

Create a Product Requirements Document (PRD) based on the feature description.

1. Review existing codebase to understand current architecture and patterns
2. Analyze the feature definition and identify unclear aspects
3. Ask the user for clarifications on aspects that significantly impact scope or user experience
4. Make reasonable decisions for minor details based on context and conventions
5. If user can't clarify, make a decision, state the assumption, and continue

Save the PRD to `{@artifacts_path}/requirements.md`.

### [x] Step: Technical Specification
<!-- chat-id: 304b232b-6ca4-4ceb-8576-1bf51ffe3731 -->

Create a technical specification based on the PRD in `{@artifacts_path}/requirements.md`.

1. Review existing codebase architecture and identify reusable components
2. Define the implementation approach

Save to `{@artifacts_path}/spec.md` with:
- Technical context (language, dependencies)
- Implementation approach referencing existing code patterns
- Source code structure changes
- Data model / API / interface changes
- Delivery phases (incremental, testable milestones)
- Verification approach using project lint/test commands

### [x] Step: Planning
<!-- chat-id: 8c9957ce-3e10-407d-b546-beead3f081e9 -->

Create a detailed implementation plan based on `{@artifacts_path}/spec.md`.

Plan created below. The spec defines 4 delivery phases. Each phase maps to one implementation step so that each step is a coherent, buildable, and testable unit. No automated unit tests exist for BattlegroundAI — verification is build + manual runtime testing — so test verification is "build compiles clean" at each step plus manual validation at the end.

### [ ] Step: Implement PickupOrb and basic ExecuteKotmoguStrategy dispatch

**Files modified:**
- `src/modules/Playerbot/PvP/BattlegroundAI.h` (add 3 new method declarations)
- `src/modules/Playerbot/PvP/BattlegroundAI.cpp` (replace 2 stubs, keep 1 stub)

**What to do:**

1. **Add method declarations to BattlegroundAI.h** (after line 301, before Silvershard Mines section):
   ```cpp
   bool HuntEnemyOrbCarrier(::Player* player);
   bool EscortOrbCarrier(::Player* player);
   bool ExecuteOrbCarrierMovement(::Player* player);
   ```

2. **Replace `PickupOrb()` stub** (line 1579-1589) with full implementation:
   - Check if player already holds an orb (check auras 121175-121178 via `player->HasAura()`)
   - Get coordinator via `sBGCoordinatorMgr->GetCoordinatorForPlayer(player)`, get TOK script via `coordinator->GetScript()` cast to `TempleOfKotmoguScript*` (verify `GetBGType() == BGType::TEMPLE_OF_KOTMOGU`)
   - Get prioritized orb list from `tokScript->GetOrbPriority(player->GetBGTeam())`
   - For each orb: skip if `tokScript->IsOrbHeld(orbId)`, get position via `tokScript->GetDynamicOrbPosition(orbId)`, compute distance
   - Select nearest unheld orb; if none available return false
   - If distance > `OBJECTIVE_RANGE` (10.0f): `BotMovementUtil::MoveToPosition(player, orbPos)` and return true
   - If within range: `player->GetGameObjectListWithEntryInGrid(goList, orbEntry, OBJECTIVE_RANGE)` where orbEntry maps to `TokData::GameObjects::{BLUE,PURPLE,GREEN,ORANGE}_ORB` (212091-212094)
   - For each GO: verify `goInfo->type == GAMEOBJECT_TYPE_FLAGSTAND`, verify distance, call `go->Use(player)`, return true
   - Add logging: attempts, target orb, distance, success/failure
   - **Follow pattern from**: `PickupFlag()` (lines 650-719) and `ExecuteFlagPickupBehavior()` (lines 2193-2264)

3. **Replace `ExecuteKotmoguStrategy()` stub** (line 1569-1577) with role-based dispatch:
   - Get coordinator and TOK script (same pattern as above)
   - Get role via `GetPlayerRole(player)` (same helper WSG uses, line 363)
   - Check if player is holding an orb via `tokScript->IsPlayerHoldingOrb(player->GetGUID())` or aura check
   - If holding orb: call `ExecuteOrbCarrierMovement(player)` (stub for now — will be implemented in Step 5)
   - Dispatch by role:
     - `ORB_CARRIER`: `PickupOrb(player)`
     - `FLAG_ESCORT`: `EscortOrbCarrier(player)` (stub for now)
     - `FLAG_HUNTER` / `NODE_ATTACKER`: `HuntEnemyOrbCarrier(player)` (stub for now)
     - `NODE_DEFENDER`: `DefendOrbCarrier(player)` (stub for now)
     - `ROAMER` / `UNASSIGNED` / default: `PickupOrb(player)` fallback
   - Log current state: player name, role, strategy, holding orb
   - **Follow pattern from**: `ExecuteWSGStrategy()` (lines 548-648)

4. **Add temporary stubs** for `HuntEnemyOrbCarrier()`, `EscortOrbCarrier()`, `ExecuteOrbCarrierMovement()` — just null check + return false. These will be filled in subsequent steps.

5. **Add orbId-to-entry mapping helper** (private inline or local lambda):
   - Maps orb IDs (0-3) to GameObject entries (212091-212094) using `TokData::GameObjects` constants

**Verification:**
- [ ] Build compiles clean: `cmake --build . --config RelWithDebInfo --target worldserver -- /maxcpucount:4`
- [ ] No warnings from modified files
- [ ] Log output shows orb pickup attempts with correct entry IDs and orb names

**Estimated scope:** ~150 lines of new implementation code

---

### [ ] Step: Implement HuntEnemyOrbCarrier and DefendOrbCarrier

**Files modified:**
- `src/modules/Playerbot/PvP/BattlegroundAI.cpp` (replace 2 stubs)

**What to do:**

1. **Replace `HuntEnemyOrbCarrier()` stub** with full implementation:
   - Get coordinator and TOK script (same pattern)
   - Find enemy orb carriers: iterate all 4 orb IDs via `tokScript->GetOrbHolder(orbId)`, for each held orb get holder via `ObjectAccessor::FindPlayer(guid)`, filter for enemy faction (`holder->IsHostileTo(player)`)
   - Select nearest enemy carrier; if multiple, prefer carriers in center zone (`tokScript->IsInCenter(x, y)` = higher priority)
   - If enemy carrier found and distance > 30.0f: `BotMovementUtil::ChaseTarget(player, enemyCarrier, 5.0f)` or `MoveToPosition()` for long distances
   - If within 30.0f: `player->SetSelection(enemyCarrier->GetGUID())` then `BotMovementUtil::ChaseTarget(player, enemyCarrier, 5.0f)`
   - If no enemy carrier found: use spatial cache `coordinator->GetNearestEnemy(playerPos, 40.0f)` and attack, or move to center
   - Add logging: target found, distance, engaging
   - **Follow pattern from**: `ExecuteFlagHunterBehavior()` (lines 2266-2293)

2. **Replace `DefendOrbCarrier()` stub** (line 1591-1598) with full implementation:
   - Get coordinator and TOK script
   - Find nearest friendly orb carrier: iterate 4 orbs via `tokScript->GetOrbHolder(orbId)`, get holders via `ObjectAccessor::FindPlayer()`, filter for same faction, pick nearest
   - If friendly carrier found and distance > 30.0f: `BotMovementUtil::MoveToPosition()` toward carrier
   - If within 30.0f: check for nearby enemies via `coordinator->QueryNearbyEnemies(carrierPos, 30.0f)`, engage nearest with `SetSelection()` + `ChaseTarget()`
   - If no enemies near carrier: maintain escort distance (8.0f behind carrier using angle math like `ExecuteEscortBehavior` line 2329-2334), with `CorrectPositionToGround()`
   - If no carrier found: patrol center area via random patrol (`BotMovementUtil::MoveRandomAround()` or angle-based patrol like `ExecuteDefenderBehavior` line 2492-2501)
   - Add logging: defending carrier X, engaging enemy Y, patrolling
   - **Follow pattern from**: `ExecuteDefenderBehavior()` (lines 2389-2506) and `ExecuteEscortBehavior()` (lines 2295-2387)

3. **Wire both into `ExecuteKotmoguStrategy()` dispatch**:
   - `FLAG_HUNTER` / `NODE_ATTACKER` now calls `HuntEnemyOrbCarrier(player)` (remove stub call)
   - `NODE_DEFENDER` now calls `DefendOrbCarrier(player)` (already wired)

**Verification:**
- [ ] Build compiles clean
- [ ] No warnings from modified files
- [ ] Bots with FLAG_HUNTER role chase and attack enemy carriers (visible in logs)
- [ ] Bots with NODE_DEFENDER role position near friendly carriers (visible in logs)

**Estimated scope:** ~150 lines of new implementation code

---

### [ ] Step: Implement EscortOrbCarrier and ExecuteOrbCarrierMovement

**Files modified:**
- `src/modules/Playerbot/PvP/BattlegroundAI.cpp` (replace 2 stubs)

**What to do:**

1. **Replace `EscortOrbCarrier()` stub** with full implementation:
   - Get coordinator and TOK script
   - Find nearest friendly orb carrier (same logic as DefendOrbCarrier step 2 — consider extracting shared helper `FindFriendlyOrbCarrier()` or inline)
   - If carrier found: get escort formation from `tokScript->GetEscortFormation(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ())`
   - Pick formation position by bot GUID index: `player->GetGUID().GetCounter() % formation.size()` (same pattern as WSG escort, line 2319)
   - `BotMovementUtil::CorrectPositionToGround(player, escortPos)` then `MoveToPosition()`
   - Fallback if no formation: offset behind carrier (angle-based positioning, same as WSG line 2329-2334)
   - If carrier is in combat (`friendlyCarrier->IsInCombat()`): use spatial cache `coordinator->QueryNearbyEnemies(carrierPos, 20.0f)` to find and target attackers with `SetSelection()` + `ChaseTarget()`
   - If no carrier found: fall back to `DefendOrbCarrier()` behavior
   - Add logging: escorting carrier X, formation position, engaging enemy
   - **Follow pattern from**: `ExecuteEscortBehavior()` (lines 2295-2387)

2. **Replace `ExecuteOrbCarrierMovement()` stub** with full implementation:
   - Get coordinator and TOK script
   - Check center push viability: `tokScript->ShouldPushToCenter(player->GetBGTeam())`
   - If should push: get route from `tokScript->GetOrbCarrierRoute(orbId)` (get orbId from `tokScript->GetPlayerOrbId(player->GetGUID())`)
   - Navigate along route waypoints with `BotMovementUtil::MoveToPosition()` — pick the furthest waypoint bot hasn't reached yet (within reasonable logic: if already past waypoint N, target N+1)
   - If in center (`tokScript->IsInCenter(x, y)`): hold position; if outnumbered (`coordinator->CountEnemiesInRadius(centerPos, 30.0f) > coordinator->CountAlliesInRadius(centerPos, 30.0f)`), retreat toward nearest ally group
   - If should NOT push: use `tokScript->GetOrbDefensePositions(orbId)` for defensive spots, move to nearest one
   - Survival priority: if health < 30% and outnumbered, move away from enemies
   - Add logging: center push decision, route progress, holding center, retreating
   - **Follow pattern from**: `ExecuteFlagCarrierBehavior()` (lines 2122-2191) for the "carry to destination" logic

3. **Complete ROAMER role in `ExecuteKotmoguStrategy()`**:
   - If unattended orb available (any orb where `!tokScript->IsOrbHeld(orbId)`): `PickupOrb(player)`
   - Otherwise: `HuntEnemyOrbCarrier(player)`

4. **Wire into dispatch**: Ensure `ExecuteKotmoguStrategy()` calls `EscortOrbCarrier()` for `FLAG_ESCORT` role, and `ExecuteOrbCarrierMovement()` when player is already holding an orb (regardless of role)

**Verification:**
- [ ] Build compiles clean
- [ ] No warnings from modified files
- [ ] Orb carriers move to center when conditions met (2+ orbs, 30s+ elapsed)
- [ ] Escorts follow carrier in formation
- [ ] Strategy adaptation visible in logs

**Estimated scope:** ~180 lines of new implementation code

---

### [ ] Step: Polish, edge cases, and full build verification

**Files modified:**
- `src/modules/Playerbot/PvP/BattlegroundAI.cpp` (modifications to all 6 functions)
- `src/modules/Playerbot/PvP/BattlegroundAI.h` (review only, no changes expected)

**What to do:**

1. **Edge case handling audit**:
   - All 4 orbs held: bots without orbs should not crash in `PickupOrb()` — must return false gracefully, dispatch falls through to defend/hunt
   - No coordinator available: all functions must have fallback behavior (basic orb pickup via grid search, basic enemy attack via `GetPlayerListInGrid`)
   - `TempleOfKotmoguScript` cast failure: verify `GetBGType()` check before cast in every function that uses the script
   - `ObjectAccessor::FindPlayer()` returns null: every call site must null-check
   - Bot already in combat: `MoveToPosition()` must not override chase movement (BotMovementUtil handles this, but verify in logic flow)
   - HEALER_SUPPORT role: add handling — prioritize escorting carrier, fall back to defend

2. **Null pointer safety audit**:
   - Walk through every function, verify: `player`, `coordinator`, `script`, `tokScript`, `go`, `goInfo`, `enemy`, `carrier`, `snapshot` — all checked before dereference
   - Check all `ObjectAccessor::FindPlayer()` results
   - Check all `coordinator->GetScript()` results

3. **Logging completeness**:
   - `ExecuteKotmoguStrategy()`: log role, strategy, holding orb (on every call)
   - `PickupOrb()`: log target orb, distance, success/failure with reason
   - `HuntEnemyOrbCarrier()`: log target found/not found, distance, engaging
   - `DefendOrbCarrier()`: log defending carrier, engaging enemy, patrolling
   - `EscortOrbCarrier()`: log carrier, formation position, combat assist
   - `ExecuteOrbCarrierMovement()`: log center push decision, route progress, holding/retreating
   - All logs use `TC_LOG_DEBUG("playerbots.bg", "[TOK] ...")` format

4. **Code style consistency**:
   - Section headers using `// ============================================================================`
   - `::Player*` with explicit global scope prefix everywhere
   - `ObjectGuid` by value, `Position` by const reference
   - No raw `new`/`delete`
   - Consistent use of `constexpr` for local constants
   - Remove any TODO comments

5. **Build verification**:
   - `cmake --build . --config RelWithDebInfo --target worldserver -- /maxcpucount:4`
   - Zero errors, zero warnings from modified files

**Verification:**
- [ ] Build compiles clean with zero warnings from modified files
- [ ] All code paths have null checks (manual audit)
- [ ] No remaining TODO comments
- [ ] All 6 functions have comprehensive logging
- [ ] HEALER_SUPPORT role handled in dispatch
- [ ] Code style matches rest of BattlegroundAI.cpp
