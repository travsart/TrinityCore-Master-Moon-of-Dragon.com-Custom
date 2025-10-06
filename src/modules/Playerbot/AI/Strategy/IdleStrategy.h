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
 * IdleStrategy - Default strategy for solo bots
 *
 * Provides basic idle behavior for bots not in a group:
 * - Wander around spawn area
 * - Interact with nearby objects/NPCs
 * - Eventually will support questing, exploration, trading
 *
 * This strategy is always active for solo bots and provides
 * a foundation for autonomous bot behavior.
 */
class TC_GAME_API IdleStrategy : public Strategy
{
public:
    IdleStrategy();
    ~IdleStrategy() override = default;

    // Strategy interface
    void InitializeActions() override;
    void InitializeTriggers() override;
    void InitializeValues() override;

    // Activation
    void OnActivate(BotAI* ai) override;
    void OnDeactivate(BotAI* ai) override;

    // Always active for solo bots
    bool IsActive(BotAI* ai) const override;

    // Update behavior
    void UpdateBehavior(BotAI* ai, uint32 diff) override;

private:
    uint32 _lastWanderTime = 0;
    uint32 _wanderInterval = 30000; // Wander every 30 seconds

    // Performance throttling - these expensive operations don't need to run every frame
    uint32 _lastQuestUpdate = 0;
    uint32 _questUpdateInterval = 2000; // Update quests every 2 seconds

    uint32 _lastGatheringUpdate = 0;
    uint32 _gatheringUpdateInterval = 1000; // Check for gathering every 1 second

    uint32 _lastTradeUpdate = 0;
    uint32 _tradeUpdateInterval = 5000; // Update trade every 5 seconds

    uint32 _lastAuctionUpdate = 0;
    uint32 _auctionUpdateInterval = 10000; // Update auction every 10 seconds
};

} // namespace Playerbot
