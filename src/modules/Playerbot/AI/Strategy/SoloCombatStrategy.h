/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Strategy.h"

namespace Playerbot
{

/**
 * @class SoloCombatStrategy
 * @brief Universal combat strategy for solo bots (not in a group)
 *
 * This strategy activates whenever a solo bot enters combat, regardless of the
 * activity that triggered it (questing, gathering, exploration, etc.). It provides
 * unified combat positioning and coordination with ClassAI for spell execution.
 *
 * **Priority**: 70 (between GroupCombat=80 and Quest=50)
 *  - Higher than non-combat activities (Quest, Loot, Gathering, Trading)
 *  - Lower than group combat (grouped bots use GroupCombatStrategy)
 *  - Combat always takes priority over non-combat activities
 *
 * **Use Cases**:
 *  - Quest combat: Quest finds target → SoloCombat positions → ClassAI casts
 *  - Gathering defense: Mob attacks while gathering → SoloCombat handles combat
 *  - Autonomous combat: Bot finds hostile → SoloCombat executes engagement
 *  - Trading interruption: Attacked at vendor → SoloCombat defends
 *
 * **Responsibilities**:
 *  - Positioning: Move bot to optimal range (melee/ranged via ClassAI)
 *  - Movement: MoveChase(target, optimalRange) for smooth following
 *  - Coordination: Let ClassAI handle spell rotation via OnCombatUpdate()
 *
 * **Performance**:
 *  - <0.1ms per update (just positioning check, no heavy computation)
 *  - Lock-free design (no mutexes, atomic operations only)
 *  - Every-frame updates when active (smooth movement)
 *
 * **Design Pattern**: Mirrors GroupCombatStrategy but for solo bots
 *
 * **Integration**: Registered in BotAI::InitializeDefaultStrategies()
 *
 * @note Does NOT handle spell casting - ClassAI::OnCombatUpdate() does that
 * @note Automatically deactivates when combat ends (IsInCombat() = false)
 * @note Deactivates when bot joins group (GroupCombatStrategy takes over)
 *
 * @see GroupCombatStrategy - Group combat equivalent
 * @see QuestStrategy - Quest objective management (non-combat)
 * @see ClassAI - Combat rotation and spell execution
 */
class TC_GAME_API SoloCombatStrategy : public Strategy
{
public:
    SoloCombatStrategy();
    virtual ~SoloCombatStrategy() = default;

    // Strategy interface implementation
    void InitializeActions() override;
    void InitializeTriggers() override;
    void InitializeValues() override;
    float GetRelevance(BotAI* ai) const override;

    /**
     * @brief Check if strategy should be active
     * @param ai Bot AI context
     * @return true if bot is solo (not in group) AND in combat
     *
     * Activation conditions:
     *  - Bot is NOT in a group (grouped bots use GroupCombatStrategy)
     *  - Bot is in combat (bot->IsInCombat() = true)
     *  - Strategy is explicitly activated (_active flag set)
     */
    bool IsActive(BotAI* ai) const override;

    /**
     * @brief Main combat positioning update (called every frame when active)
     * @param ai Bot AI context
     * @param diff Time delta since last update (milliseconds)
     *
     * Execution flow:
     *  1. Validate bot and target (GetVictim())
     *  2. Get optimal range from ClassAI (melee vs ranged)
     *  3. If not already chasing, issue MoveChase() command
     *  4. ClassAI::OnCombatUpdate() handles spell casting
     *
     * Performance: <0.1ms per call (just positioning logic)
     */
    void UpdateBehavior(BotAI* ai, uint32 diff) override;

private:
    /**
     * @brief Get optimal combat range for the bot's class
     * @param ai Bot AI context
     * @param target Combat target
     * @return Optimal range in yards (5.0 for melee, 25.0 for ranged, or class-specific)
     *
     * Tries to use ClassAI::GetOptimalRange() if available, otherwise uses class-based defaults:
     *  - Melee classes (Warrior, Rogue, Paladin, DK, Monk, Feral): 5.0 yards
     *  - Ranged classes (Hunter, Mage, Warlock, Priest): 25.0 yards
     *  - Hybrid classes: ClassAI determines optimal range based on spec
     */
    float GetOptimalCombatRange(BotAI* ai, Unit* target) const;
};

} // namespace Playerbot
