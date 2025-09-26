/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

// Combat/ThreatManager.h removed - not used in this file
#include "BotAI.h"
#include "Player.h"
#include "Log.h"

// Include all class-specific AI headers
#include "ClassAI/Warriors/WarriorAI.h"
#include "ClassAI/Paladins/PaladinAI.h"
#include "ClassAI/Hunters/HunterAI.h"
#include "ClassAI/Rogues/RogueAI.h"
#include "ClassAI/Priests/PriestAI.h"
#include "ClassAI/Shamans/ShamanAI.h"
#include "ClassAI/Mages/MageAI.h"
#include "ClassAI/Warlocks/WarlockAI.h"
#include "ClassAI/Monks/MonkAI.h"
#include "ClassAI/Druids/DruidAI.h"
#include "ClassAI/DemonHunters/DemonHunterAI.h"
#include "ClassAI/DeathKnights/DeathKnightAI.h"
#include "ClassAI/Evokers/EvokerAI.h"

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

    TC_LOG_DEBUG("module.playerbot.ai", "Creating class-specific AI for player {} (class: {})",
                 bot->GetName(), bot->GetClass());

    // Create class-specific AI instead of generic BotAI
    return CreateClassAI(bot, bot->GetClass());
}

std::unique_ptr<BotAI> BotAIFactory::CreateClassAI(Player* bot, uint8 classId)
{
    if (!bot)
    {
        TC_LOG_ERROR("module.playerbot.ai", "BotAIFactory::CreateClassAI called with null player");
        return nullptr;
    }

    try
    {
        std::unique_ptr<BotAI> botAI;

        switch (classId)
        {
            case CLASS_WARRIOR:
                botAI = std::make_unique<WarriorAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created WarriorAI for player {}", bot->GetName());
                break;
            case CLASS_PALADIN:
                botAI = std::make_unique<PaladinAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created PaladinAI for player {}", bot->GetName());
                break;
            case CLASS_HUNTER:
                botAI = std::make_unique<HunterAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created HunterAI for player {}", bot->GetName());
                break;
            case CLASS_ROGUE:
                botAI = std::make_unique<RogueAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created RogueAI for player {}", bot->GetName());
                break;
            case CLASS_PRIEST:
                botAI = std::make_unique<PriestAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created PriestAI for player {}", bot->GetName());
                break;
            case CLASS_SHAMAN:
                botAI = std::make_unique<ShamanAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created ShamanAI for player {}", bot->GetName());
                break;
            case CLASS_MAGE:
                botAI = std::make_unique<MageAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created MageAI for player {}", bot->GetName());
                break;
            case CLASS_WARLOCK:
                botAI = std::make_unique<WarlockAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created WarlockAI for player {}", bot->GetName());
                break;
            case CLASS_MONK:
                botAI = std::make_unique<MonkAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created MonkAI for player {}", bot->GetName());
                break;
            case CLASS_DRUID:
                botAI = std::make_unique<DruidAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created DruidAI for player {}", bot->GetName());
                break;
            case CLASS_DEMON_HUNTER:
                botAI = std::make_unique<DemonHunterAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created DemonHunterAI for player {}", bot->GetName());
                break;
            case CLASS_DEATH_KNIGHT:
                botAI = std::make_unique<DeathKnightAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created DeathKnightAI for player {}", bot->GetName());
                break;
            case CLASS_EVOKER:
                botAI = std::make_unique<EvokerAI>(bot);
                TC_LOG_INFO("module.playerbot.ai", "Created EvokerAI for player {}", bot->GetName());
                break;
            default:
                TC_LOG_WARN("module.playerbot.ai", "Unknown class {} for player {}, creating DefaultBotAI",
                           classId, bot->GetName());
                botAI = std::make_unique<DefaultBotAI>(bot);
                break;
        }

        return botAI;
    }
    catch (const std::exception& e)
    {
        TC_LOG_ERROR("module.playerbot.ai", "Exception creating class AI for player {}: {}",
                     bot->GetName(), e.what());
        TC_LOG_WARN("module.playerbot.ai", "Falling back to DefaultBotAI for player {}", bot->GetName());
        return std::make_unique<DefaultBotAI>(bot);
    }
    catch (...)
    {
        TC_LOG_ERROR("module.playerbot.ai", "Unknown exception creating class AI for player {}",
                     bot->GetName());
        TC_LOG_WARN("module.playerbot.ai", "Falling back to DefaultBotAI for player {}", bot->GetName());
        return std::make_unique<DefaultBotAI>(bot);
    }
}

std::unique_ptr<BotAI> BotAIFactory::CreateClassAI(Player* bot, uint8 classId, uint8 spec)
{
    // For now, create based on class and let the class AI handle specialization internally
    return CreateClassAI(bot, classId);
}

std::unique_ptr<BotAI> BotAIFactory::CreateSpecializedAI(Player* bot, std::string const& type)
{
    // Implement specialized AI types as needed
    return CreateClassAI(bot, bot->GetClass());
}

std::unique_ptr<BotAI> BotAIFactory::CreatePvPAI(Player* bot)
{
    // For now, use class-specific AI with PvP strategies
    return CreateClassAI(bot, bot->GetClass());
}

std::unique_ptr<BotAI> BotAIFactory::CreatePvEAI(Player* bot)
{
    // For now, use class-specific AI with PvE strategies
    return CreateClassAI(bot, bot->GetClass());
}

std::unique_ptr<BotAI> BotAIFactory::CreateRaidAI(Player* bot)
{
    // For now, use class-specific AI with raid strategies
    return CreateClassAI(bot, bot->GetClass());
}

} // namespace Playerbot