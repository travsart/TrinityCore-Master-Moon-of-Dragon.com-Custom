/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

// Combat/ThreatManager.h removed - not used in this file
#include "BotAI.h"
#include "Player.h"
#include "Log.h"

namespace Playerbot {

BotAIFactory* BotAIFactory::instance()
{
    static BotAIFactory _instance;
    return &_instance;
}

std::unique_ptr<BotAI> BotAIFactory::CreateAI(Player* bot)
{
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot.ai", "BotAIFactory::CreateAI called with null player");
        return nullptr;
    }

    TC_LOG_DEBUG("module.playerbot.ai", "Creating BotAI for player {} (class: {})",
                 bot->GetName(), bot->GetClass());

    try
    {
        // Create basic BotAI instance
        auto botAI = std::make_unique<BotAI>(bot);

        TC_LOG_DEBUG("module.playerbot.ai", "Successfully created BotAI for player {}", bot->GetName());
        return botAI;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("module.playerbot.ai", "Exception creating BotAI for player {}: {}",
                     bot->GetName(), e.what());
        return nullptr;
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.ai", "Unknown exception creating BotAI for player {}",
                     bot->GetName());
        return nullptr;
    }
}

} // namespace Playerbot