# System Architecture Overview

## Module Structure

### Core Components (src/modules/Playerbot/)
```
Playerbot/
├── Core/               # Core bot framework
│   ├── References/     # SafeObjectReference system
│   ├── Managers/       # Manager registry and base interfaces
│   └── Events/         # Event dispatcher
├── Session/            # Bot session management
├── Account/            # Bot account management
├── Character/          # Character creation and distribution
├── Lifecycle/          # Bot spawning and lifecycle
├── AI/                 # AI systems
│   ├── ClassAI/        # Class-specific AI (13 classes)
│   ├── Strategy/       # Strategy pattern implementations
│   ├── Actions/        # Action system
│   ├── Triggers/       # Trigger system
│   ├── Combat/         # Combat coordination
│   └── CombatBehaviors/# Combat behavior utilities
├── Movement/           # Movement and pathfinding
├── Game/               # Game system integration
├── Quest/              # Quest system
├── Economy/            # Auction house integration
├── Social/             # Trade and guild systems
├── Professions/        # Profession automation
├── Equipment/          # Equipment management
├── Performance/        # Performance optimization
├── Database/           # Database access layer
├── Config/             # Configuration system
├── Scripts/            # TrinityCore script integration
├── Events/             # Event system
└── Tests/              # Unit and integration tests
```

## Key Design Patterns

### 1. Manager Pattern (BehaviorManager)
Base class for all coordinated subsystems:
- QuestManager
- TradeManager
- AuctionManager
- GatheringManager
- InventoryManager
- GroupCoordinator

### 2. Strategy Pattern
AI decision-making:
- CombatStrategy
- IdleStrategy
- QuestStrategy
- GroupCombatStrategy
- CombatMovementStrategy

### 3. Hook Pattern (Core Integration)
Minimal core integration via hooks:
```cpp
// In core file (minimal change)
if (PlayerBotHooks::OnPlayerLogin)
    PlayerBotHooks::OnPlayerLogin(this);

// In module (all logic)
class PlayerBotHooks {
    static void OnPlayerLogin(Player* player) {
        // All bot logic here
    }
};
```

### 4. Observer Pattern (Event System)
Event flow: TrinityCore ScriptMgr → PlayerbotEventScripts → EventDispatcher → Managers

### 5. Factory Pattern
- BotAIFactory: Creates class-specific AI instances
- BotSessionFactory: Creates bot sessions

## Class AI Architecture

All 13 WoW classes supported with template-based specializations:
- Death Knight (Blood, Frost, Unholy)
- Demon Hunter (Havoc, Vengeance)
- Druid (Balance, Feral, Guardian, Restoration)
- Evoker (Devastation, Preservation)
- Hunter (Beast Mastery, Marksmanship, Survival)
- Mage (Arcane, Fire, Frost)
- Monk (Brewmaster, Mistweaver, Windwalker)
- Paladin (Holy, Protection, Retribution)
- Priest (Discipline, Holy, Shadow)
- Rogue (Assassination, Outlaw, Subtlety)
- Shaman (Elemental, Enhancement, Restoration)
- Warlock (Affliction, Demonology, Destruction)
- Warrior (Arms, Fury, Protection)

## Performance Architecture

### Threading
- Intel TBB for parallel algorithms
- Thread pools for bot operations
- Lock-free data structures (parallel-hashmap)
- Minimal lock contention

### Memory Management
- Object pooling for frequent allocations
- Smart pointer usage (std::unique_ptr, std::shared_ptr)
- Memory per bot: <10MB target

### Database
- Connection pooling
- Query optimization
- Result caching
- Async queries with Boost.Asio

## Integration Points

### TrinityCore Integration
- PlayerbotModule: Module loader entry point
- PlayerbotEventScripts: Script system integration
- BotSession: Extends WorldSession concept
- BotAI: Integrates with TrinityCore's AI framework

### Database Schema
- playerbot_playerbot: Bot-specific data
- playerbot_auth: Bot account data
- playerbot_characters: Bot character data (shares with main characters DB)
- playerbot_world: Bot world state (if applicable)

## Configuration System

All configuration in `playerbots.conf`:
- Core settings (enable/disable, max bots, update intervals)
- AI behavior tuning
- Performance thresholds
- Database connection
- Security settings
- Social features
- Experimental features