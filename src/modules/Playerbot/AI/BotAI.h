/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#ifndef BOT_AI_H
#define BOT_AI_H

#include "Define.h"
#include <memory>

// Forward declarations
class Player;

namespace Playerbot {

/**
 * @brief Base AI interface for bot behavior
 *
 * This is a minimal interface for AI integration with BotSession.
 * Complete AI implementation will be done in later phases.
 */
class TC_GAME_API BotAI
{
public:
    explicit BotAI(Player* player) : _player(player) {}
    virtual ~BotAI() = default;

    /**
     * @brief Update AI logic
     * @param diff Time difference since last update in milliseconds
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Get the player this AI controls
     */
    Player* GetPlayer() const { return _player; }

protected:
    Player* _player;
};

/**
 * @brief Default AI implementation for basic bot behavior
 */
class TC_GAME_API DefaultBotAI : public BotAI
{
public:
    explicit DefaultBotAI(Player* player) : BotAI(player) {}

    void Update(uint32 diff) override
    {
        // Minimal AI - just stand idle for now
        // Complete AI implementation in later phases
    }
};

} // namespace Playerbot

#endif // BOT_AI_H