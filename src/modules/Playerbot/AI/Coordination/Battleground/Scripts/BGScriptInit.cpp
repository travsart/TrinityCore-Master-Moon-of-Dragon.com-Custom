/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "BGScriptInit.h"
#include "BGScriptRegistry.h"
#include "Log.h"

// Force linker to include BG script object files by declaring extern references
// to symbols in each script file. This prevents MSVC from discarding "unused" object files.

// Forward declare registration functions from each BG script
namespace Playerbot::Coordination::Battleground
{
    // These are dummy functions that exist solely to create a reference
    // to each BG script's object file, preventing linker discard
    namespace BGScriptLinkerForce
    {
        // Domination BGs
        void ForceIncludeTempleOfKotmoguScript();
        // Add more as we create more BG scripts:
        // void ForceIncludeArathiBasinScript();
        // void ForceIncludeGilneasScript();
        // etc.
    }
}

// Implementation of force-include functions (defined in each script's .cpp)
// We don't actually call them - just referencing them is enough

namespace Playerbot::Coordination::Battleground
{

void InitializeBGScripts()
{
    TC_LOG_INFO("playerbots.bg.script", "InitializeBGScripts: Forcing linker inclusion of BG scripts...");

    // Reference the force-include functions to prevent linker from discarding them
    // We use a volatile pointer to prevent the optimizer from removing this
    volatile void* forceInclude[] = {
        reinterpret_cast<void*>(&BGScriptLinkerForce::ForceIncludeTempleOfKotmoguScript),
        // Add more as we create more BG scripts
    };
    (void)forceInclude;  // Suppress unused variable warning

    // Log what scripts are registered
    auto& registry = BGScriptRegistry::Instance();
    TC_LOG_INFO("playerbots.bg.script",
        "InitializeBGScripts: {} BG scripts registered", registry.GetScriptCount());

    registry.LogRegisteredScripts();
}

} // namespace Playerbot::Coordination::Battleground
