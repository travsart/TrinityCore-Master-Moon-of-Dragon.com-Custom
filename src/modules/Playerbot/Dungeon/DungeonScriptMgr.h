/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include "Core/DI/Interfaces/IDungeonScriptMgr.h"
#include "DungeonScript.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace Playerbot
{

/**
 * @brief Dungeon Script Manager - Registry for dungeon scripts
 *
 * Implements IDungeonScriptMgr for dependency injection compatibility.
 * This class manages all dungeon scripts using a plugin-style architecture
 * similar to TrinityCore's ScriptMgr. Scripts are registered at server startup
 * and looked up at runtime.
 *
 * ARCHITECTURE:
 * - Scripts register themselves via RegisterScript()
 * - Lookups by mapId or bossId for O(1) access
 * - Falls back to generic mechanics if no script found
 * - Thread-safe registration and lookup
 *
 * USAGE:
 * // Registration (in DungeonScriptLoader.cpp):
 * DungeonScriptMgr::instance()->RegisterScript(new DeadminesScript());
 *
 * // Lookup (in DungeonBehavior.cpp):
 * DungeonScript* script = DungeonScriptMgr::instance()->
 *     GetScriptForMap(player->GetMapId());
 *
 * // Execution with fallback:
 * if (script)
 *     script->HandleInterruptPriority(player, boss);
 * else
 *     EncounterStrategy::HandleGenericInterrupts(player, boss);
 */
class TC_GAME_API DungeonScriptMgr final : public IDungeonScriptMgr
{
public:
    static DungeonScriptMgr* instance();

    // ============================================================================
    // IDungeonScriptMgr interface implementation
    // ============================================================================

    /**
     * Initialize script manager (called once at startup)
     */
    void Initialize() override;

    /**
     * Load all dungeon scripts
     * Called by Initialize() to register scripts
     */
    void LoadScripts() override;

    // ============================================================================
    // SCRIPT REGISTRATION
    // ============================================================================

    /**
     * Register a dungeon script
     * @param script Script to register (manager takes ownership)
     */
    void RegisterScript(DungeonScript* script) override;

    /**
     * Register boss entry to script mapping
     * @param bossEntry Creature entry ID for boss
     * @param script Script handling this boss
     */
    void RegisterBossScript(uint32 bossEntry, DungeonScript* script) override;

    // ============================================================================
    // SCRIPT LOOKUP
    // ============================================================================

    /**
     * Get script for map ID
     * @param mapId Map ID to look up
     * @return Script pointer or nullptr if not found
     */
    DungeonScript* GetScriptForMap(uint32 mapId) const override;

    /**
     * Get script for boss entry
     * @param bossEntry Creature entry ID
     * @return Script pointer or nullptr if not found
     */
    DungeonScript* GetScriptForBoss(uint32 bossEntry) const override;

    /**
     * Check if script exists for map
     */
    bool HasScriptForMap(uint32 mapId) const override;

    /**
     * Check if script exists for boss
     */
    bool HasScriptForBoss(uint32 bossEntry) const override;

    // ============================================================================
    // MECHANIC EXECUTION (with fallback)
    // ============================================================================

    /**
     * Execute boss mechanic with automatic fallback
     *
     * This method provides the THREE-LEVEL FALLBACK system:
     * 1. Try script-specific override
     * 2. Try script base class (calls generic)
     * 3. Fall back to direct generic call
     *
     * @param player Player executing mechanic
     * @param boss Boss being fought
     * @param mechanic Mechanic type to execute
     */
    void ExecuteBossMechanic(::Player* player, ::Creature* boss, MechanicType mechanic) override;

    // ============================================================================
    // STATISTICS
    // ============================================================================

    /**
     * Get number of registered scripts
     */
    uint32 GetScriptCount() const override { return _scriptCount.load(); }

    /**
     * Get number of registered boss mappings
     */
    uint32 GetBossMappingCount() const override { return _bossMappingCount.load(); }

    /**
     * Get statistics on script usage
     */
    using ScriptStats = IDungeonScriptMgr::ScriptStats;

    ScriptStats GetStats() const override;

    // ============================================================================
    // DEBUGGING
    // ============================================================================

    /**
     * List all registered scripts (for debugging)
     */
    void ListAllScripts() const override;

    /**
     * Get script info by name
     */
    DungeonScript* GetScriptByName(::std::string const& name) const override;

private:
    DungeonScriptMgr();
    ~DungeonScriptMgr();

    // Prevent copying
    DungeonScriptMgr(DungeonScriptMgr const&) = delete;
    DungeonScriptMgr& operator=(DungeonScriptMgr const&) = delete;

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    // Map scripts (mapId -> script) - OWNS the scripts via unique_ptr (primary storage)
    ::std::unordered_map<uint32, ::std::unique_ptr<DungeonScript>> _mapScripts;

    // Boss scripts (bossEntry -> script) - NON-OWNING raw pointers (lookup only)
    ::std::unordered_map<uint32, DungeonScript*> _bossScripts;

    // Script name lookup (name -> script) - NON-OWNING raw pointers (lookup only)
    ::std::unordered_map<::std::string, DungeonScript*> _namedScripts;

    // Statistics
    ::std::atomic<uint32> _scriptCount{0};
    ::std::atomic<uint32> _bossMappingCount{0};
    ::std::atomic<uint32> _scriptHits{0};
    ::std::atomic<uint32> _scriptMisses{0};
    ::std::atomic<uint32> _mechanicExecutions{0};

    mutable Playerbot::OrderedRecursiveMutex<Playerbot::LockOrder::BEHAVIOR_MANAGER> _mutex;
    bool _initialized;
};

} // namespace Playerbot
