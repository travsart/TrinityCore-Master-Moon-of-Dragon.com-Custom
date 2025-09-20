# TrinityCore Module Development Guide

## Universal ModuleManager API

This guide documents the newly created ModuleManager API, which provides a reliable alternative to ScriptMgr for module lifecycle management in TrinityCore.

### Background

ScriptMgr's WorldScript lifecycle methods (OnStartup, OnUpdate, OnShutdown) were found to have inconsistent behavior. Some methods like OnConfigLoad work reliably, but OnStartup and OnUpdate calls don't always reach registered scripts. This ModuleManager API was created to provide guaranteed lifecycle event delivery for all TrinityCore modules.

### Features

- **Reliable Lifecycle Events**: Guaranteed OnStartup, OnUpdate, and OnShutdown calls
- **Simple Registration**: Easy module registration with std::function callbacks
- **Module Management**: Enable/disable modules at runtime
- **Debug Support**: List registered modules and check registration status
- **Thread-Safe**: Designed for TrinityCore's multi-threaded environment

### API Reference

#### Core Files

```
src/server/game/Modules/ModuleManager.h
src/server/game/Modules/ModuleManager.cpp
```

#### Module Interface

```cpp
struct ModuleInterface
{
    std::string name;
    std::function<void()> onStartup;
    std::function<void(uint32)> onUpdate;
    std::function<void()> onShutdown;
    bool enabled = true;
};
```

#### Core Methods

```cpp
class TC_GAME_API ModuleManager
{
public:
    // Register a module for lifecycle events
    static void RegisterModule(std::string const& name,
                              std::function<void()> onStartup,
                              std::function<void(uint32)> onUpdate,
                              std::function<void()> onShutdown);

    // Enable/disable a registered module
    static void SetModuleEnabled(std::string const& name, bool enabled);

    // Internal lifecycle methods (called by TrinityCore)
    static void CallOnStartup();
    static void CallOnUpdate(uint32 diff);
    static void CallOnShutdown();

    // Status and debugging
    static std::vector<std::string> GetRegisteredModules();
    static bool IsModuleRegistered(std::string const& name);
};
```

### Integration Points

The ModuleManager is integrated into TrinityCore at these key points:

#### Main.cpp
```cpp
// After ScriptMgr startup
sScriptMgr->OnStartup();

// Initialize registered modules
ModuleManager::CallOnStartup();
```

#### World.cpp
```cpp
// In world update cycle
{
    TC_METRIC_TIMER("world_update_time", TC_METRIC_TAG("type", "Update modules"));
    ModuleManager::CallOnUpdate(diff);
}

// During shutdown
ModuleManager::CallOnShutdown();
```

### Usage Example: Playerbot Integration

Here's how the Playerbot module integrates with ModuleManager:

#### 1. Create Module Adapter

```cpp
// PlayerbotModuleAdapter.h
namespace Playerbot
{
    class TC_GAME_API PlayerbotModuleAdapter
    {
    public:
        static void RegisterWithModuleManager();

    private:
        static void OnModuleStartup();
        static void OnModuleUpdate(uint32 diff);
        static void OnModuleShutdown();

        static bool s_registered;
        static bool s_initialized;
    };
}
```

#### 2. Implement Lifecycle Methods

```cpp
// PlayerbotModuleAdapter.cpp
void PlayerbotModuleAdapter::RegisterWithModuleManager()
{
    if (s_registered)
        return;

    TC_LOG_INFO("module.playerbot", "Registering with ModuleManager");

    ModuleManager::RegisterModule("Playerbot",
        OnModuleStartup,
        OnModuleUpdate,
        OnModuleShutdown);

    s_registered = true;
}

void PlayerbotModuleAdapter::OnModuleStartup()
{
    // Module initialization logic
    if (!sPlayerbotConfig || !sPlayerbotConfig->GetBool("Playerbot.Enable", false))
        return;

    // Initialize your module systems here
    s_initialized = true;
}

void PlayerbotModuleAdapter::OnModuleUpdate(uint32 diff)
{
    if (!s_initialized)
        return;

    // Update your module systems here
    if (sBotSpawner)
        sBotSpawner->Update(diff);
}

void PlayerbotModuleAdapter::OnModuleShutdown()
{
    if (!s_initialized)
        return;

    // Clean shutdown of module systems
    s_initialized = false;
}
```

#### 3. Register During Module Initialization

```cpp
// In PlayerbotModule::Initialize()
void PlayerbotModule::RegisterHooks()
{
    // Register with ModuleManager for reliable lifecycle management
    TC_LOG_INFO("server.loading", "Registering Playerbot with ModuleManager...");
    Playerbot::PlayerbotModuleAdapter::RegisterWithModuleManager();
    TC_LOG_INFO("server.loading", "Playerbot successfully registered with ModuleManager");
}
```

### Development Guidelines

#### 1. Module Registration

- Register during module initialization, not during global construction
- Use descriptive module names (e.g., "Playerbot", "CustomModule")
- Check if already registered to avoid duplicate registration

#### 2. Lifecycle Method Implementation

- **OnStartup**: Initialize module systems after world is fully loaded
- **OnUpdate**: Update active module systems, check enabled state first
- **OnShutdown**: Clean shutdown of all module resources

#### 3. Error Handling

```cpp
void MyModule::OnModuleUpdate(uint32 diff)
{
    if (!s_initialized)
        return;

    try
    {
        // Your update logic here
        MySystem::Update(diff);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("module.mymodule", "Update failed: {}", e.what());
    }
}
```

#### 4. Configuration Integration

```cpp
void MyModule::OnModuleStartup()
{
    // Check if module is enabled via configuration
    if (!sMyModuleConfig || !sMyModuleConfig->GetBool("MyModule.Enable", false))
    {
        TC_LOG_INFO("module.mymodule", "Module disabled in configuration");
        return;
    }

    // Initialize module systems
    s_initialized = true;
}
```

### Best Practices

#### Performance Considerations

- Keep OnUpdate methods lightweight (< 1ms per call)
- Use static update counters for periodic operations
- Avoid heavy database operations in update loops

```cpp
void MyModule::OnModuleUpdate(uint32 diff)
{
    static uint32 periodicTimer = 0;
    periodicTimer += diff;

    if (periodicTimer >= 5000) // Every 5 seconds
    {
        PerformPeriodicTask();
        periodicTimer = 0;
    }
}
```

#### Logging Standards

- Use module-specific log categories: "module.mymodule"
- Log registration and initialization events at INFO level
- Log errors at ERROR level with context
- Use DEBUG level for detailed operation logging

#### Thread Safety

- ModuleManager is thread-safe for registration and status queries
- Ensure your module's update methods are thread-safe
- Use appropriate locking for shared module resources

#### Configuration Management

- Create separate configuration files for each module
- Use module-specific configuration sections
- Validate configuration during startup

### Debugging and Monitoring

#### Check Module Registration

```cpp
// Check if module is registered
if (ModuleManager::IsModuleRegistered("MyModule"))
{
    TC_LOG_INFO("module.mymodule", "Module is registered");
}

// List all registered modules
std::vector<std::string> modules = ModuleManager::GetRegisteredModules();
for (auto const& module : modules)
{
    TC_LOG_INFO("module.manager", "Registered module: {}", module);
}
```

#### Runtime Module Control

```cpp
// Disable module at runtime
ModuleManager::SetModuleEnabled("MyModule", false);

// Re-enable module
ModuleManager::SetModuleEnabled("MyModule", true);
```

### Migration from ScriptMgr

If you have existing WorldScript implementations, migration is straightforward:

#### Before (WorldScript)
```cpp
class MyWorldScript : public WorldScript
{
public:
    MyWorldScript() : WorldScript("MyWorldScript") { }

    void OnStartup() override { /* startup code */ }
    void OnUpdate(uint32 diff) override { /* update code */ }
    void OnShutdown() override { /* shutdown code */ }
};
```

#### After (ModuleManager)
```cpp
class MyModuleAdapter
{
public:
    static void RegisterWithModuleManager()
    {
        ModuleManager::RegisterModule("MyModule",
            OnModuleStartup,
            OnModuleUpdate,
            OnModuleShutdown);
    }

private:
    static void OnModuleStartup() { /* startup code */ }
    static void OnModuleUpdate(uint32 diff) { /* update code */ }
    static void OnModuleShutdown() { /* shutdown code */ }
};
```

### Troubleshooting

#### Common Issues

1. **Module not receiving updates**: Check if OnStartup was called and s_initialized is true
2. **Registration failures**: Ensure registration happens after world initialization
3. **Performance issues**: Profile update methods, implement periodic timers for heavy operations

#### Debug Logging

Enable detailed module logging by setting log level to DEBUG for module categories:

```
Logger.modules.level=2
Logger.module.mymodule.level=2
```

### Conclusion

The ModuleManager API provides a robust, reliable alternative to ScriptMgr for module lifecycle management. It guarantees that your module will receive all lifecycle events and provides tools for runtime management and debugging.

For any issues or questions about the ModuleManager API, refer to the implementation in `src/server/game/Modules/` or the Playerbot module integration example.

---

**Created**: September 2024
**Last Updated**: September 2024
**Tested With**: TrinityCore master branch
**Dependencies**: C++20, TrinityCore game library