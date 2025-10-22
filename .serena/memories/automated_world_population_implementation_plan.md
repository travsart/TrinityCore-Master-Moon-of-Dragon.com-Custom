# Automated World Population System - Implementation Plan

## Project Status
**Phase**: Ready for Implementation
**Documents Created**:
- AUTOMATED_WORLD_POPULATION_DESIGN.md (src/modules/Playerbot/)
- THREAD_SAFE_POPULATION_DESIGN.md (src/modules/Playerbot/)

## Critical Design Decisions

### 1. Bot Creation Workflow (CONFIRMED WITH USER)

**Level Brackets (Every 5 levels)**:
- Bracket 1: Level 1-4 (5%) - Natural leveling, NO instant gear
- Bracket 2-16: Level 5-79 (6% each) - Instant level-up + full gear
- Bracket 17: Level 80 (13%) - Endgame concentration
- Total: 17 brackets = 100%

**Workflow Steps**:
1. Determine bot level from distribution brackets
2. IF level >= 5: Instant level-up (bot->GiveLevel + InitTalentForLevel + SetXP(0))
3. IF level >= 5: Choose 1st spec, apply talents
4. IF level >= 10: Choose 2nd spec, apply talents (WoW 11.2 dual-spec unlock)
5. IF level >= 5: Add 4x bags, bot equips bags
6. IF level >= 5: Equip gear for spec 1 (active), add spec 2 gear to bags (unequipped)
7. IF level >= 5: Add consumables (food, water, potions)
8. Teleport to appropriate zone (starter zone for 1-4, level-appropriate for 5+)
9. Save to database, add to world

### 2. Item Distribution (CONFIRMED WITH USER)

**Quality Mix**:
- Leveling (1-59): 50% Green / 50% Blue
- Pre-Endgame (60-79): 30% Green / 70% Blue
- Endgame (80): 60% Blue / 40% Purple

**Selection Method**:
- 100% automated, ZERO manual curation
- Database queries from item_template
- Stat-weighted scoring per spec (Str > Crit for Warriors, etc.)
- Pick from top 3 candidates (randomized for realism)
- Important slots (weapons, chest, legs) get 70% chance for higher quality

**Guaranteed Coverage**:
- All 17 equipment slots filled
- Fallback weapon if query fails (starting weapon)
- Broader queries if specific ilvl range empty

### 3. Distribution Tolerance (CONFIRMED)

**±15% Tolerance**:
- Target 10% → Accept 8.5%-11.5%
- Formula: lowerBound = target * 0.85, upperBound = target * 1.15
- Applies to percentage of total bots, not level range

### 4. Thread Safety Architecture (CRITICAL)

**Lock-Free Components**:
- Gear cache: Immutable after startup build
- Talent cache: Immutable after startup build
- Zone cache: Immutable after startup build
- Distribution counters: std::atomic<uint32> with relaxed memory order

**Two-Phase Creation**:
- Phase 1 (Worker Thread): PrepareBotData() - compute gear, talents, location
- Phase 2 (Main Thread): ApplyBotPlan() - TrinityCore API calls

**ThreadPool Integration**:
- Use existing Playerbot::Performance::ThreadPool
- Submit tasks with TaskPriority::NORMAL
- Work-stealing queue for load balancing

## TrinityCore Native Leveling (CRITICAL)

**Correct Pattern (Matches GM .levelup command)**:
```cpp
// 1. Set specialization FIRST (before GiveLevel)
ApplySpecialization(bot, specId);

// 2. Instant level-up (learns ALL skills/spells for levels 1-target)
bot->GiveLevel(targetLevel);

// 3. Initialize talent framework (sends client data)
bot->InitTalentForLevel();

// 4. Reset XP
bot->SetXP(0);

// 5. Apply specific talent loadout
ApplyTalentLoadout(bot, specId, targetLevel);
```

**Why InitTalentForLevel() called twice?**:
- GiveLevel() calls it internally
- GM command calls it AGAIN to ensure SendTalentsInfoData() updates client UI
- This guarantees talent frame synchronization for online players

## Key Data Structures

### LevelBracket (Thread-Safe)
```cpp
struct LevelBracket {
    uint32 minLevel;
    uint32 maxLevel;
    float targetPercentage;
    TeamId faction;
    
    // THREAD-SAFE: Atomic counter
    std::atomic<uint32> currentCount{0};
    
    void IncrementCount() {
        currentCount.fetch_add(1, std::memory_order_relaxed);
    }
    
    bool IsNaturalLeveling() const {
        return minLevel <= 4;
    }
    
    bool SupportsDualSpec() const {
        return minLevel >= 10;
    }
};
```

### BotCreationPlan (Two-Phase Data)
```cpp
struct BotCreationPlan {
    // Input
    uint32 accountId;
    std::string name;
    Races race;
    Classes cls;
    Gender gender;
    uint32 targetLevel;
    uint32 spec1;
    uint32 spec2;
    
    // Output (computed in worker thread)
    GearSet spec1Gear;
    GearSet spec2Gear;
    std::vector<uint32> talents1;
    std::vector<uint32> talents2;
    std::vector<uint32> bags;
    WorldLocation spawnLocation;
    
    // Status
    bool preparationComplete{false};
    std::string errorMessage;
};
```

### GearSet
```cpp
struct GearSet {
    std::map<EquipmentSlots, uint32> items; // slot -> item entry
    uint32 averageIlvl;
    float totalScore;
    
    bool HasWeapon() const {
        return items.count(EQUIPMENT_SLOT_MAINHAND) > 0;
    }
    
    bool IsComplete() const {
        return items.size() >= 10;
    }
};
```

## Component Architecture

### New Components to Create

1. **BotLevelDistribution** (src/modules/Playerbot/Character/)
   - LoadConfig() - Read brackets from playerbots.conf
   - SelectBracket() - Choose bracket based on current distribution
   - GetOverpopulatedBrackets() - Find brackets above tolerance
   - AnalyzeCurrentDistribution() - Update counters

2. **BotLevelManager** (src/modules/Playerbot/Lifecycle/)
   - DetermineInitialLevel() - Select level from distribution
   - CreateBotsAsync() - Submit batch to thread pool
   - Update() - Process main-thread queue
   - CheckAndRebalance() - Periodic distribution check

3. **BotGearFactory** (src/modules/Playerbot/Equipment/)
   - Initialize() - Build immutable gear cache
   - BuildGearSet() - Query & score items for spec
   - EquipBotForLevel() - Apply gear to bot
   - GetAppropriateBags() - Select bags by level

4. **BotTalentManager** (src/modules/Playerbot/Talents/)
   - LoadTalentLoadouts() - Read talent builds from DB
   - ApplySpecialization() - Set bot spec
   - ApplyTalentLoadout() - Apply talent points
   - ActivateSpecialization() - Switch active spec

5. **BotWorldPositioner** (src/modules/Playerbot/Movement/)
   - BuildZoneCache() - Load zone-to-level mappings
   - TeleportBotForLevel() - Teleport to appropriate zone
   - TeleportToStarterZone() - For level 1-4 bots

## Configuration (playerbots.conf)

```conf
# Enable system
Playerbot.Population.Enabled = 1

# Number of brackets
Playerbot.Population.NumBrackets = 17

# Rebalance intervals
Playerbot.Population.RebalanceInterval = 300
Playerbot.Population.FlaggedCheckInterval = 15
Playerbot.Population.MaxFlaggedPerCycle = 5

# Dynamic distribution
Playerbot.Population.DynamicDistribution = 0
Playerbot.Population.RealPlayerWeight = 1.0
Playerbot.Population.SyncFactions = 0

# Exclusions
Playerbot.Population.ExcludeGuildBots = 1
Playerbot.Population.ExcludeFriendListed = 1

# Thread pool
Playerbot.Population.WorkerThreads = 0
Playerbot.Population.BatchSize = 50
Playerbot.Population.MaxApplyPerUpdate = 10
Playerbot.Population.TaskPriority = NORMAL

# Alliance brackets (17 total)
Playerbot.Population.Alliance.Range1.Min = 1
Playerbot.Population.Alliance.Range1.Max = 4
Playerbot.Population.Alliance.Range1.Pct = 5

Playerbot.Population.Alliance.Range2.Min = 5
Playerbot.Population.Alliance.Range2.Max = 9
Playerbot.Population.Alliance.Range2.Pct = 6

# ... continues every 5 levels ...

Playerbot.Population.Alliance.Range17.Min = 80
Playerbot.Population.Alliance.Range17.Max = 80
Playerbot.Population.Alliance.Range17.Pct = 13

# Horde brackets (same structure)
```

## Database Schema

### New Tables

```sql
-- Talent loadouts (auto-generated or manually curated)
CREATE TABLE IF NOT EXISTS `playerbot_talent_loadouts` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `class_id` TINYINT UNSIGNED NOT NULL,
  `spec_id` TINYINT UNSIGNED NOT NULL,
  `min_level` TINYINT UNSIGNED NOT NULL,
  `max_level` TINYINT UNSIGNED NOT NULL,
  `talent_string` TEXT NOT NULL COMMENT 'Comma-separated talent entry IDs',
  `hero_talent_string` TEXT COMMENT 'Hero talents for 71+',
  `description` VARCHAR(255),
  PRIMARY KEY (`id`),
  KEY `idx_class_spec_level` (`class_id`, `spec_id`, `min_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Zone level mappings (optional - can build from creature data)
CREATE TABLE IF NOT EXISTS `playerbot_zone_levels` (
  `zone_id` INT UNSIGNED NOT NULL,
  `area_id` INT UNSIGNED,
  `min_level` TINYINT UNSIGNED NOT NULL,
  `max_level` TINYINT UNSIGNED NOT NULL,
  `faction` TINYINT UNSIGNED COMMENT '0=neutral, 1=Alliance, 2=Horde',
  PRIMARY KEY (`zone_id`),
  KEY `idx_level` (`min_level`, `max_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Guild tracking (for exclusions)
CREATE TABLE IF NOT EXISTS `playerbot_guild_real_players` (
  `guild_id` INT UNSIGNED NOT NULL,
  `has_real_players` TINYINT(1) NOT NULL DEFAULT 0,
  `last_updated` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## Item Level Mapping (WoW 11.2)

```cpp
static const std::map<uint32, uint32> levelToIlvl = {
    // Leveling 1-70
    {1,   5},    {5,   10},   {10,  20},   {15,  30},   {20,  40},
    {25,  50},   {30,  60},   {35,  70},   {40,  80},   {45,  90},
    {50, 100},   {55, 110},   {60, 120},   {65, 130},   {70, 140},
    
    // War Within (71-80)
    {71, 558},   {72, 562},   {73, 566},   {74, 570},   {75, 574},
    {76, 578},   {77, 582},   {78, 586},   {79, 590},
    
    // Endgame
    {80, 593}    // Normal Dungeon baseline
};
```

## Performance Targets

- **Bot Creation Rate**: 25 bots/second (1500/minute)
- **Memory per Bot**: <10MB
- **Cache Build Time**: <5 seconds at startup
- **Distribution Check**: <100ms every 5 minutes
- **Thread Contention**: <1% (lock-free design)

## Implementation Phases

### Phase 1: Foundation (Week 1-2)
- [ ] BotLevelDistribution class + config loading
- [ ] Bracket selection algorithm
- [ ] Distribution tolerance checking

### Phase 2: Gear & Talents (Week 2-3)
- [ ] BotGearFactory with immutable cache
- [ ] Item query & scoring system
- [ ] BotTalentManager with loadout DB
- [ ] Dual-spec support

### Phase 3: World Positioning (Week 3-4)
- [ ] BotWorldPositioner with zone cache
- [ ] Starter zone teleports
- [ ] Level-appropriate zone selection

### Phase 4: Thread Pool Integration (Week 4-5)
- [ ] Two-phase bot creation
- [ ] Worker thread preparation
- [ ] Main thread application queue
- [ ] Atomic counter updates

### Phase 5: Testing & Tuning (Week 5-6)
- [ ] Thread safety tests
- [ ] Stress tests (1000+ bots)
- [ ] Performance profiling
- [ ] Distribution balance verification

## Critical Code Patterns

### Main Thread Task Queue
```cpp
std::queue<std::function<void()>> _mainThreadTasks;
std::mutex _mainThreadMutex;

void ScheduleMainThreadTask(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(_mainThreadMutex);
    _mainThreadTasks.push(std::move(task));
}

void Update(uint32 diff) {
    std::unique_lock<std::mutex> lock(_mainThreadMutex);
    uint32 processed = 0;
    
    while (!_mainThreadTasks.empty() && processed < MAX_PER_UPDATE) {
        auto task = std::move(_mainThreadTasks.front());
        _mainThreadTasks.pop();
        
        lock.unlock();
        task();
        lock.lock();
        
        ++processed;
    }
}
```

### ThreadPool Submission
```cpp
auto& threadPool = GetThreadPool(ThreadPoolType::GENERAL);

threadPool.Submit(
    [this, accountId, targetLevel]() {
        // Worker thread
        BotCreationPlan plan = PrepareBotData(accountId, targetLevel);
        
        // Schedule main thread
        ScheduleMainThreadTask([this, plan]() {
            ApplyBotPlan(plan);
        });
    },
    TaskPriority::NORMAL
);
```

### Immutable Cache Access
```cpp
// Build once at startup (single-threaded)
void BuildCache() {
    for (uint32 level = 1; level <= 80; ++level) {
        // ... query and populate _cache ...
    }
    _initialized.store(true, std::memory_order_release);
}

// Read anywhere (lock-free)
std::vector<ItemScore> const* GetCachedItems(...) const {
    while (!_initialized.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return &_cache[key];
}
```

## Next Steps After Reboot

1. Read this memory: `mcp__serena__read_memory("automated_world_population_implementation_plan")`
2. Review design documents in src/modules/Playerbot/
3. Start with Phase 1: BotLevelDistribution implementation
4. Follow the implementation checklist
5. Test each phase before moving to next

## Key Reminders

- ✅ Use TrinityCore's native GiveLevel() - don't reinvent leveling
- ✅ Level 1-4 bots get NO gear, level naturally
- ✅ Level 5+ get instant level-up + full gear
- ✅ Dual-spec at level 10+ (WoW 11.2 behavior)
- ✅ Thread-safe: atomic counters, immutable caches, two-phase creation
- ✅ Zero manual curation - 100% automated gear selection
- ✅ ±15% distribution tolerance
- ✅ Main thread serialization for TrinityCore API calls
