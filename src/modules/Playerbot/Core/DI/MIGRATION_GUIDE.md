# Dependency Injection Migration Guide

**Document Version:** 5.4
**Last Updated:** 2025-11-08
**Status:** Phase 45 Complete (60 of 168 singletons migrated)

---

## Overview

The Playerbot module is transitioning from Meyer's Singleton pattern to Dependency Injection (DI) for improved testability and maintainability.

**Benefits:**
- **Testability**: Services can be mocked for unit testing
- **Visibility**: Dependencies are explicit in constructors
- **Flexibility**: Service implementations can be swapped
- **Thread-safety**: Uses OrderedMutex for deadlock prevention

---

## Migration Status

| Service | Interface | Status | Migration Method |
|---------|-----------|--------|------------------|
| **SpatialGridManager** | ISpatialGridManager | ✅ Phase 1 | Dual-access (singleton + DI) |
| **BotSessionMgr** | IBotSessionMgr | ✅ Phase 1 | Dual-access (singleton + DI) |
| **ConfigManager** | IConfigManager | ✅ Phase 2 | Dual-access (singleton + DI) |
| **BotLifecycleManager** | IBotLifecycleManager | ✅ Phase 2 | Dual-access (singleton + DI) |
| **BotDatabasePool** | IBotDatabasePool | ✅ Phase 2 | Dual-access (singleton + DI) |
| **BotNameMgr** | IBotNameMgr | ✅ Phase 3 | Dual-access (singleton + DI) |
| **DungeonScriptMgr** | IDungeonScriptMgr | ✅ Phase 3 | Dual-access (singleton + DI) |
| **EquipmentManager** | IEquipmentManager | ✅ Phase 4 | Dual-access (singleton + DI) |
| **BotAccountMgr** | IBotAccountMgr | ✅ Phase 4 | Dual-access (singleton + DI) |
| **LFGBotManager** | ILFGBotManager | ✅ Phase 5 | Dual-access (singleton + DI) |
| **BotGearFactory** | IBotGearFactory | ✅ Phase 5 | Dual-access (singleton + DI) |
| **BotMonitor** | IBotMonitor | ✅ Phase 6 | Dual-access (singleton + DI) |
| **BotLevelManager** | IBotLevelManager | ✅ Phase 6 | Dual-access (singleton + DI) |
| **PlayerbotGroupManager** | IPlayerbotGroupManager | ✅ Phase 7 | Dual-access (singleton + DI) |
| **BotTalentManager** | IBotTalentManager | ✅ Phase 7 | Dual-access (singleton + DI) |
| **PlayerbotConfig** | IPlayerbotConfig | ✅ Phase 8 | Dual-access (singleton + DI) |
| **BotSpawner** | IBotSpawner | ✅ Phase 8 | Dual-access (singleton + DI) |
| **BotWorldPositioner** | IBotWorldPositioner | ✅ Phase 9 | Dual-access (singleton + DI) |
| **BotHealthCheck** | IBotHealthCheck | ✅ Phase 9 | Dual-access (singleton + DI) |
| **BotScheduler** | IBotScheduler | ✅ Phase 10 | Dual-access (singleton + DI) |
| **BotCharacterDistribution** | IBotCharacterDistribution | ✅ Phase 11 | Dual-access (singleton + DI) |
| **BotLevelDistribution** | IBotLevelDistribution | ✅ Phase 11 | Dual-access (singleton + DI) |
| **GroupEventBus** | IGroupEventBus | ✅ Phase 12 | Dual-access (singleton + DI) |
| **LFGGroupCoordinator** | ILFGGroupCoordinator | ✅ Phase 12 | Dual-access (singleton + DI) |
| **LootEventBus** | ILootEventBus | ✅ Phase 13 | Dual-access (singleton + DI) |
| **QuestEventBus** | IQuestEventBus | ✅ Phase 13 | Dual-access (singleton + DI) |
| **AuctionEventBus** | IAuctionEventBus | ✅ Phase 14 | Dual-access (singleton + DI) |
| **NPCEventBus** | INPCEventBus | ✅ Phase 14 | Dual-access (singleton + DI) |
| **CooldownEventBus** | ICooldownEventBus | ✅ Phase 15 | Dual-access (singleton + DI) |
| **AuraEventBus** | IAuraEventBus | ✅ Phase 15 | Dual-access (singleton + DI) |
| **InstanceEventBus** | IInstanceEventBus | ✅ Phase 16 | Dual-access (singleton + DI) |
| **SocialEventBus** | ISocialEventBus | ✅ Phase 17 | Dual-access (singleton + DI) |
| **CombatEventBus** | ICombatEventBus | ✅ Phase 18 | Dual-access (singleton + DI) |
| **ResourceEventBus** | IResourceEventBus | ✅ Phase 19 | Dual-access (singleton + DI) |
| **LootAnalysis** | ILootAnalysis | ✅ Phase 20 | Dual-access (singleton + DI) |
| **GuildBankManager** | IGuildBankManager | ✅ Phase 21 | Dual-access (singleton + DI) |
| **LootCoordination** | ILootCoordination | ✅ Phase 22 | Dual-access (singleton + DI) |
| **LootDistribution** | ILootDistribution | ✅ Phase 23 | Dual-access (singleton + DI) |
| **MarketAnalysis** | IMarketAnalysis | ✅ Phase 24 | Dual-access (singleton + DI) |
| **TradeSystem** | ITradeSystem | ✅ Phase 25 | Dual-access (singleton + DI) |
| **QuestPickup** | IQuestPickup | ✅ Phase 26 | Dual-access (singleton + DI) |
| **GuildEventCoordinator** | IGuildEventCoordinator | ✅ Phase 27 | Dual-access (singleton + DI) |
| **ProfessionManager** | IProfessionManager | ✅ Phase 28 | Dual-access (singleton + DI) |
| **QuestCompletion** | IQuestCompletion | ✅ Phase 29 | Dual-access (singleton + DI) |
| **QuestValidation** | IQuestValidation | ✅ Phase 30 | Dual-access (singleton + DI) |
| **QuestTurnIn** | IQuestTurnIn | ✅ Phase 31 | Dual-access (singleton + DI) |
| **RoleAssignment** | IRoleAssignment | ✅ Phase 32 | Dual-access (singleton + DI) |
| **DynamicQuestSystem** | IDynamicQuestSystem | ✅ Phase 33 | Dual-access (singleton + DI) |
| **FarmingCoordinator** | IFarmingCoordinator | ✅ Phase 34 | Dual-access (singleton + DI) |
| **AuctionHouse** | IAuctionHouse | ✅ Phase 35 | Dual-access (singleton + DI) |
| **ProfessionAuctionBridge** | IProfessionAuctionBridge | ✅ Phase 36 | Dual-access (singleton + DI) |
| **LFGRoleDetector** | ILFGRoleDetector | ✅ Phase 37 | Dual-access (singleton + DI) |
| **VendorInteraction** | IVendorInteraction | ✅ Phase 38 | Dual-access (singleton + DI) |
| **LFGBotSelector** | ILFGBotSelector | ✅ Phase 39 | Dual-access (singleton + DI) |
| **GuildIntegration** | IGuildIntegration | ✅ Phase 40 | Dual-access (singleton + DI) |
| **DungeonBehavior** | IDungeonBehavior | ✅ Phase 41 | Dual-access (singleton + DI) |
| **InstanceCoordination** | IInstanceCoordination | ✅ Phase 42 | Dual-access (singleton + DI) |
| **EncounterStrategy** | IEncounterStrategy | ✅ Phase 43 | Dual-access (singleton + DI) |
| **ObjectiveTracker** | IObjectiveTracker | ✅ Phase 44 | Dual-access (singleton + DI) |
| **UnifiedInterruptSystem** | IUnifiedInterruptSystem | ✅ Phase 45 | Dual-access (singleton + DI) |
| *+108 more* | *TBD* | ⏳ Pending | Planned Phases 46-N |

**Total Progress:** 60/168 singletons (35.7%)

---

## Quick Start

### 1. Register Services (Startup)

Add to `World::SetInitialWorldSettings()`:

```cpp
#include "Core/DI/ServiceRegistration.h"

void World::SetInitialWorldSettings()
{
    // ... existing code ...

    // Register Playerbot services
    Playerbot::RegisterPlayerbotServices();
}
```

### 2. Using Services (Old Way - Still Works)

```cpp
// Old singleton access (backward compatible)
auto grid = sSpatialGridManager.GetGrid(mapId);
auto session = sBotSessionMgr->GetSession(accountId);
```

### 3. Using Services (New Way - Recommended)

```cpp
// New DI access
auto spatialMgr = Playerbot::Services::Container().Resolve<ISpatialGridManager>();
auto grid = spatialMgr->GetGrid(mapId);

auto sessionMgr = Playerbot::Services::Container().Resolve<IBotSessionMgr>();
auto session = sessionMgr->GetSession(accountId);
```

### 4. Constructor Injection (New Code)

```cpp
class BotAI
{
public:
    // Dependencies visible in constructor (preferred)
    BotAI(Player* bot,
          std::shared_ptr<ISpatialGridManager> spatialMgr,
          std::shared_ptr<IBotSessionMgr> sessionMgr)
        : _bot(bot)
        , _spatialMgr(spatialMgr)
        , _sessionMgr(sessionMgr)
    {}

    void FindNearestEnemy()
    {
        // Use injected dependencies (mockable!)
        auto grid = _spatialMgr->GetGrid(GetMap()->GetId());
        // ...
    }

private:
    Player* _bot;
    std::shared_ptr<ISpatialGridManager> _spatialMgr;
    std::shared_ptr<IBotSessionMgr> _sessionMgr;
};
```

---

## Unit Testing with Mocks

### Example: Testing BotAI with Mock Services

```cpp
#include "Tests/Mocks/MockSpatialGridManager.h"
#include <catch2/catch.hpp>

TEST_CASE("BotAI finds nearest enemy", "[botai]")
{
    using namespace Playerbot::Testing;

    // Create mocks
    auto mockSpatialMgr = std::make_shared<MockSpatialGridManager>();
    auto mockSessionMgr = std::make_shared<MockBotSessionMgr>();

    // Optional: Configure mock behavior
    mockSpatialMgr->SetMockGrid(0, nullptr);  // Map 0 has no grid

    // Create BotAI with mocks (constructor injection)
    BotAI ai(testBot, mockSpatialMgr, mockSessionMgr);

    // Execute test
    ai.FindNearestEnemy();

    // Verify mock was called
    REQUIRE(mockSpatialMgr->WasGetGridCalled());
    REQUIRE(mockSpatialMgr->GetGetGridCallCount() == 1);
    REQUIRE(mockSpatialMgr->GetLastQueriedMapId() == 0);
}
```

### Example: Testing with DI Container

```cpp
TEST_CASE("BotAI uses spatial grid from container", "[botai][integration]")
{
    using namespace Playerbot;
    using namespace Playerbot::Testing;

    // Clear container (test isolation)
    Services::Container().Clear();

    // Register mocks in container
    auto mockSpatialMgr = std::make_shared<MockSpatialGridManager>();
    Services::Container().RegisterInstance<ISpatialGridManager>(mockSpatialMgr);

    // Code under test resolves from container
    auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
    REQUIRE(spatialMgr != nullptr);

    // Verify it's our mock
    auto grid = spatialMgr->GetGrid(0);
    REQUIRE(mockSpatialMgr->WasGetGridCalled());

    // Cleanup
    Services::Container().Clear();
}
```

---

## Migration Patterns

### Pattern 1: Service Locator (Transitional)

**Use Case:** Existing code that uses singletons

**Before:**
```cpp
void BotAI::Update()
{
    auto grid = sSpatialGridManager.GetGrid(mapId);  // Singleton
}
```

**After (Service Locator):**
```cpp
void BotAI::Update()
{
    auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
    auto grid = spatialMgr->GetGrid(mapId);
}
```

**Pros:**
- Minimal code changes
- Gradual migration
- Services are mockable

**Cons:**
- Hidden dependencies (not in constructor)
- Service Locator is anti-pattern (better than singleton, but not ideal)

---

### Pattern 2: Constructor Injection (Preferred)

**Use Case:** New code or refactored classes

**Before:**
```cpp
class BotAI
{
public:
    BotAI(Player* bot) : _bot(bot) {}

    void Update()
    {
        auto grid = sSpatialGridManager.GetGrid(mapId);  // Hidden dependency
    }

private:
    Player* _bot;
};
```

**After (Constructor Injection):**
```cpp
class BotAI
{
public:
    // Dependencies explicit (visible, testable)
    BotAI(Player* bot, std::shared_ptr<ISpatialGridManager> spatialMgr)
        : _bot(bot), _spatialMgr(spatialMgr)
    {}

    void Update()
    {
        auto grid = _spatialMgr->GetGrid(mapId);  // Injected dependency
    }

private:
    Player* _bot;
    std::shared_ptr<ISpatialGridManager> _spatialMgr;  // Explicit dependency
};
```

**Factory:**
```cpp
class BotAIFactory
{
public:
    static std::unique_ptr<BotAI> Create(Player* bot)
    {
        // Resolve dependencies from container
        auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();

        return std::make_unique<BotAI>(bot, spatialMgr);
    }
};
```

**Pros:**
- Dependencies visible in constructor (self-documenting)
- Fully testable (mocks can be injected)
- Follows SOLID principles

**Cons:**
- Requires more refactoring
- Changes constructor signatures

---

## ServiceContainer API

### Registration

```cpp
// Register singleton with implementation type
Services::Container().RegisterSingleton<IInterface, Implementation>();

// Register existing instance
auto instance = std::make_shared<Implementation>();
Services::Container().RegisterInstance<IInterface>(instance);

// Register factory (lazy initialization)
Services::Container().RegisterFactory<IInterface>([]() {
    return std::make_shared<Implementation>(dependencies...);
});
```

### Resolution

```cpp
// Resolve service (returns nullptr if not registered)
auto service = Services::Container().Resolve<IInterface>();
if (service)
{
    service->DoSomething();
}

// Require service (throws exception if not registered)
auto service = Services::Container().RequireService<IInterface>();
service->DoSomething();  // No null check needed

// Check if registered
if (Services::Container().IsRegistered<IInterface>())
{
    // ...
}
```

### Management

```cpp
// Unregister service
Services::Container().Unregister<IInterface>();

// Clear all services
Services::Container().Clear();

// Get service count
size_t count = Services::Container().GetServiceCount();
```

---

## Interface Design Guidelines

### 1. Pure Abstract Interface

```cpp
class ISomeService
{
public:
    virtual ~ISomeService() = default;

    // Pure virtual methods only
    virtual void DoSomething() = 0;
    virtual int GetValue() const = 0;
};
```

### 2. Implementation Inherits Interface

```cpp
class SomeService final : public ISomeService
{
public:
    // ISomeService implementation
    void DoSomething() override { /* ... */ }
    int GetValue() const override { return _value; }

private:
    int _value = 0;
};
```

### 3. Avoid Concrete Dependencies

**Bad:**
```cpp
class BotAI
{
    SpatialGridManager* _spatialMgr;  // ❌ Concrete dependency
};
```

**Good:**
```cpp
class BotAI
{
    std::shared_ptr<ISpatialGridManager> _spatialMgr;  // ✅ Interface dependency
};
```

---

## Common Pitfalls

### 1. Forgetting to Register Services

**Problem:**
```cpp
auto service = Services::Container().Resolve<IMyService>();
// service is nullptr - service was never registered!
```

**Solution:**
```cpp
// In ServiceRegistration.h
container.RegisterSingleton<IMyService, MyService>();
```

### 2. Circular Dependencies

**Problem:**
```cpp
class ServiceA : public IServiceA
{
    std::shared_ptr<IServiceB> _serviceB;  // A depends on B
};

class ServiceB : public IServiceB
{
    std::shared_ptr<IServiceA> _serviceA;  // B depends on A - CIRCULAR!
};
```

**Solution:**
- Redesign to remove circular dependency
- Use factory pattern or late binding
- Consider if one service should own the other

### 3. Singleton + DI Mismatch

**Problem:**
```cpp
// Service registered in DI container
Services::Container().RegisterInstance<IMyService>(myService);

// But code still uses singleton!
auto service = sMyService.DoSomething();  // Bypasses DI!
```

**Solution:**
- Use DI consistently
- Remove singleton accessor after migration complete
- During transition, both work (dual-access pattern)

---

## Migration Checklist

### For Each Service:

- [ ] Create interface (e.g., `ISomeService`)
- [ ] Make implementation inherit interface
- [ ] Add `override` keywords to all virtual methods
- [ ] Register service in `ServiceRegistration.h`
- [ ] Create mock implementation in `Tests/Mocks/`
- [ ] Update callers to use DI (service locator or constructor injection)
- [ ] Write unit tests with mocks
- [ ] Mark singleton accessor as deprecated (optional)
- [ ] Eventually remove singleton accessor (after full migration)

---

## Performance Considerations

### Shared Pointer Overhead

**Question:** Does `std::shared_ptr` impact performance?

**Answer:** Minimal impact for long-lived services:
- Service lookup: ~100ns (map lookup + atomic increment)
- Service usage: Zero overhead (direct method call)
- Service lifetime: Singleton services live for entire server lifetime

**Recommendation:** Use `std::shared_ptr` for services, raw pointers for entities.

### Caching Resolved Services

**Bad (repeated resolution):**
```cpp
void Update()
{
    // Resolves every frame - wasteful
    auto service = Services::Container().Resolve<IMyService>();
    service->DoSomething();
}
```

**Good (cache in member):**
```cpp
class MyClass
{
public:
    MyClass()
        : _myService(Services::Container().Resolve<IMyService>())
    {}

    void Update()
    {
        // Use cached service - zero overhead
        _myService->DoSomething();
    }

private:
    std::shared_ptr<IMyService> _myService;
};
```

---

## Next Steps

### Phase 2 (COMPLETED ✅)
- ✅ Migrated 3 additional core services (ConfigManager, BotLifecycleManager, BotDatabasePool)
- ✅ Created 3 new interfaces (IConfigManager, IBotLifecycleManager, IBotDatabasePool)
- ✅ Updated ServiceRegistration with Phase 2 services
- ✅ Created MockConfigManager for testing

### Phase 3 (COMPLETED ✅)
- ✅ Migrated 2 high-priority singletons (BotNameMgr, DungeonScriptMgr)
- ✅ Created 2 new interfaces (IBotNameMgr, IDungeonScriptMgr)
- ✅ Updated ServiceRegistration with Phase 3 services
- ✅ Fixed copyright headers (TrinityCore GPL v2)

### Phase 4 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (EquipmentManager, BotAccountMgr)
- ✅ Created 2 new interfaces (IEquipmentManager, IBotAccountMgr)
- ✅ Updated ServiceRegistration with Phase 4 services
- ✅ Total progress: 9/168 singletons (5.4%)

### Phase 5 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (LFGBotManager, BotGearFactory)
- ✅ Created 2 new interfaces (ILFGBotManager, IBotGearFactory)
- ✅ Updated ServiceRegistration with Phase 5 services
- ✅ Total progress: 11/168 singletons (6.5%)

### Phase 6 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (BotMonitor, BotLevelManager)
- ✅ Created 2 new interfaces (IBotMonitor, IBotLevelManager)
- ✅ Updated ServiceRegistration with Phase 6 services
- ✅ Total progress: 13/168 singletons (7.7%)

### Phase 7 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (PlayerbotGroupManager, BotTalentManager)
- ✅ Created 2 new interfaces (IPlayerbotGroupManager, IBotTalentManager)
- ✅ Updated ServiceRegistration with Phase 7 services
- ✅ Total progress: 15/168 singletons (8.9%)

### Phase 8 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (PlayerbotConfig, BotSpawner)
- ✅ Created 2 new interfaces (IPlayerbotConfig, IBotSpawner)
- ✅ Updated ServiceRegistration with Phase 8 services
- ✅ Total progress: 17/168 singletons (10.1%)

### Phase 9 (COMPLETED ✅)
- ✅ Migrated 2 additional high-priority singletons (BotWorldPositioner, BotHealthCheck)
- ✅ Created 2 new interfaces (IBotWorldPositioner, IBotHealthCheck)
- ✅ Updated ServiceRegistration with Phase 9 services
- ✅ Total progress: 19/168 singletons (11.3%)

### Phase 10 (COMPLETED ✅)
- ✅ Migrated 1 additional high-priority singleton (BotScheduler)
- ✅ Created 1 new interface (IBotScheduler)
- ✅ Updated ServiceRegistration with Phase 10 service
- ✅ Total progress: 20/168 singletons (11.9%)

### Phase 11 (Planned)
- Migrate 2+ additional high-priority singletons
- Expand unit test coverage with additional mocks
- Create integration test examples

### Phase 12+ (Future)
- Migrate remaining 148 singletons incrementally
- Remove singleton accessors (backward compatibility break)
- Full constructor injection throughout codebase

---

## FAQ

**Q: Do I have to use DI for all new code?**
A: Recommended but not required. DI makes testing easier, but singleton still works for now.

**Q: Can I still use `sSpatialGridManager`?**
A: Yes! During transition, both singleton and DI work. Eventually singletons will be deprecated.

**Q: How do I test code that uses singletons?**
A: Refactor to use DI (service locator or constructor injection), then mock in tests.

**Q: What if a service isn't registered?**
A: `Resolve()` returns `nullptr`. Use `RequireService()` to throw exception if missing.

**Q: Is this compatible with TrinityCore?**
A: Yes! DI is isolated to Playerbot module. TrinityCore code unchanged.

---

**Document Owner:** Playerbot Architecture Team
**Questions?** File an issue on GitHub or ask in Discord
