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
 * @class GroupCombatStrategy
 * @brief Minimal strategy that makes bots attack when group members are in combat
 *
 * This strategy simply monitors group combat state and triggers ClassAI combat behaviors.
 * It doesn't implement complex actions - just coordinates existing bot combat systems.
 */
class TC_GAME_API GroupCombatStrategy : public Strategy
{
public:
    GroupCombatStrategy();
    virtual ~GroupCombatStrategy() = default;

    // Strategy interface
    void InitializeActions() override;
    void InitializeTriggers() override;
    void InitializeValues() override;
    float GetRelevance(BotAI* ai) const override;

    // CRITICAL FIX: UpdateBehavior is called every frame, not GetRelevance!
    void UpdateBehavior(BotAI* ai, uint32 diff) override;

private:
    bool IsGroupInCombat(BotAI* ai) const;
};

} // namespace Playerbot