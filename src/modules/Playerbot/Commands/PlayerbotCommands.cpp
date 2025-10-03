/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 */

#include "Chat.h"
#include "ChatCommands/ChatCommand.h"
#include "Language.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "CharacterCache.h"
#include "RBAC.h"
#include "Lifecycle/BotSpawner.h"
#include "Lifecycle/BotCharacterCreator.h"
#include "Session/BotWorldSessionMgr.h"
#include "Session/BotSession.h"
#include "AI/BotAI.h"
#include "Config/PlayerbotConfig.h"
#include <sstream>

using namespace Trinity::ChatCommands;

// Custom RBAC permissions (1000+ range for custom modules)
namespace rbac
{
    enum PlayerbotRBACPermissions
    {
        RBAC_PERM_COMMAND_PLAYERBOT_ADD        = 1000,
        RBAC_PERM_COMMAND_PLAYERBOT_REMOVE     = 1001,
        RBAC_PERM_COMMAND_PLAYERBOT_LIST       = 1002,
        RBAC_PERM_COMMAND_PLAYERBOT_INFO       = 1003,
        RBAC_PERM_COMMAND_PLAYERBOT_STATS      = 1004,
        RBAC_PERM_COMMAND_PLAYERBOT_FOLLOW     = 1005,
        RBAC_PERM_COMMAND_PLAYERBOT_STAY       = 1006,
        RBAC_PERM_COMMAND_PLAYERBOT_ATTACK     = 1007,
        RBAC_PERM_COMMAND_PLAYERBOT_DEFEND     = 1008,
        RBAC_PERM_COMMAND_PLAYERBOT_HEAL       = 1009,
        RBAC_PERM_COMMAND_PLAYERBOT_DEBUG      = 1010,
        RBAC_PERM_COMMAND_PLAYERBOT_SPAWN      = 1011
    };
}

class playerbot_commandscript : public CommandScript
{
public:
    playerbot_commandscript() : CommandScript("playerbot_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable pbotCommandTable =
        {
            { "add",       HandlePlayerbotAddCommand,       rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "remove",    HandlePlayerbotRemoveCommand,    rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "list",      HandlePlayerbotListCommand,      rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "info",      HandlePlayerbotInfoCommand,      rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "stats",     HandlePlayerbotStatsCommand,     rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "follow",    HandlePlayerbotFollowCommand,    rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "stay",      HandlePlayerbotStayCommand,      rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "attack",    HandlePlayerbotAttackCommand,    rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "defend",    HandlePlayerbotDefendCommand,    rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "heal",      HandlePlayerbotHealCommand,      rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "debug",     HandlePlayerbotDebugCommand,     rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
            { "spawn",     HandlePlayerbotSpawnCommand,     rbac::RBAC_PERM_COMMAND_PET_CREATE,  Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "bot",  pbotCommandTable },
            { "pbot", pbotCommandTable },
        };
        return commandTable;
    }

    // .bot add <characterName> - Add existing character as bot
    static bool HandlePlayerbotAddCommand(ChatHandler* handler, std::string characterName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sPlayerbotConfig || !sPlayerbotConfig->GetBool("Playerbot.Enable", false))
        {
            handler->SendSysMessage("PlayerBot system is disabled in configuration.");
            return true;
        }

        // Check if character exists
        ObjectGuid charGuid = sCharacterCache->GetCharacterGuidByName(characterName);
        if (charGuid.IsEmpty())
        {
            handler->PSendSysMessage("Character '%s' not found.", characterName.c_str());
            return true;
        }

        // Add as bot
        if (Playerbot::sBotWorldSessionMgr->AddPlayerBot(charGuid, player->GetSession()->GetAccountId()))
        {
            handler->PSendSysMessage("Bot '%s' added successfully!", characterName.c_str());
        }
        else
        {
            handler->PSendSysMessage("Failed to add bot '%s'. Check logs for details.", characterName.c_str());
        }

        return true;
    }

    // .bot remove <name|all> - Remove specific bot or all bots
    static bool HandlePlayerbotRemoveCommand(ChatHandler* handler, std::string target)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();

        if (target == "all")
        {
            // Remove all bots for this account
            Playerbot::sBotWorldSessionMgr->RemoveAllPlayerBots(accountId);
            handler->SendSysMessage("Removed all your bots.");
        }
        else
        {
            // Remove specific bot by name
            Player* bot = ObjectAccessor::FindPlayerByName(target);
            if (!bot)
            {
                handler->PSendSysMessage("Bot '%s' not found or not online.", target.c_str());
                return true;
            }

            // Verify ownership
            if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
            {
                handler->SendSysMessage("You don't own this bot.");
                return true;
            }

            Playerbot::sBotWorldSessionMgr->RemovePlayerBot(bot->GetGUID());
            handler->PSendSysMessage("Bot '%s' removed.", target.c_str());
        }

        return true;
    }

    // .bot list - List all your bots
    static bool HandlePlayerbotListCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();
        std::vector<Player*> bots = Playerbot::sBotWorldSessionMgr->GetPlayerBotsByAccount(accountId);

        if (bots.empty())
        {
            handler->SendSysMessage("You have no active bots.");
            return true;
        }

        handler->PSendSysMessage("Your active bots (%u):", static_cast<uint32>(bots.size()));
        for (Player* bot : bots)
        {
            if (bot)
            {
                handler->PSendSysMessage("  - %s (Level %u %s)",
                    bot->GetName().c_str(),
                    bot->GetLevel(),
                    GetClassName(bot->GetClass()));
            }
        }

        return true;
    }

    // .bot info <name> - Display detailed bot information
    static bool HandlePlayerbotInfoCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // Find bot by name
        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify this is a bot owned by the player
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        // Display bot information
        handler->PSendSysMessage("=== Bot Info: %s ===", bot->GetName().c_str());
        handler->PSendSysMessage("Level: %u %s %s",
            bot->GetLevel(),
            GetRaceName(bot->GetRace()),
            GetClassName(bot->GetClass()));
        handler->PSendSysMessage("Health: %u/%u", bot->GetHealth(), bot->GetMaxHealth());
        handler->PSendSysMessage("Power: %u/%u %s",
            bot->GetPower(bot->GetPowerType()),
            bot->GetMaxPower(bot->GetPowerType()),
            GetPowerName(bot->GetPowerType()));

        if (botAI)
        {
            handler->PSendSysMessage("AI State: Active");
            handler->PSendSysMessage("In Combat: %s", botAI->IsInCombat() ? "Yes" : "No");
        }
        else
        {
            handler->SendSysMessage("AI State: Inactive");
        }

        if (bot->GetMap())
            handler->PSendSysMessage("Location: Map %u, Zone %u",
                bot->GetMapId(), bot->GetZoneId());

        return true;
    }

    // .bot stats - Display bot system statistics
    static bool HandlePlayerbotStatsCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 accountBotCount = Playerbot::sBotWorldSessionMgr->GetBotCountByAccount(accountId);
        uint32 totalBotCount = Playerbot::sBotWorldSessionMgr->GetBotCount();

        handler->SendSysMessage("=== PlayerBot Statistics ===");
        handler->PSendSysMessage("Your active bots: %u", accountBotCount);
        handler->PSendSysMessage("Server total bots: %u", totalBotCount);

        if (sPlayerbotConfig)
        {
            uint32 maxBots = sPlayerbotConfig->GetUInt("Playerbot.MaxBots", 500);
            uint32 maxPerAccount = sPlayerbotConfig->GetUInt("Playerbot.MaxBotsPerAccount", 10);
            handler->PSendSysMessage("Max bots per account: %u", maxPerAccount);
            handler->PSendSysMessage("Max server bots: %u", maxBots);
        }

        return true;
    }

    // .bot follow <name> - Make bot follow you
    static bool HandlePlayerbotFollowCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI and set follow mode
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            botAI->Follow(player, 5.0f);
            botAI->SetAIState(Playerbot::BotAIState::FOLLOWING);
            handler->PSendSysMessage("Bot '%s' is now following you.", botName.c_str());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot stay <name> - Make bot stay at current location
    static bool HandlePlayerbotStayCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI and set stay mode
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            botAI->StopMovement();
            botAI->SetAIState(Playerbot::BotAIState::IDLE);
            handler->PSendSysMessage("Bot '%s' is now staying.", botName.c_str());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot attack <name> - Make bot attack your target
    static bool HandlePlayerbotAttackCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get player's target
        Unit* target = player->GetSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage("You need to select a target first.");
            return true;
        }

        // Get bot AI and set attack target
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            botAI->SetTarget(target->GetGUID());
            bot->Attack(target, true);
            handler->PSendSysMessage("Bot '%s' is now attacking %s.",
                botName.c_str(), target->GetName().c_str());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot defend <name> - Make bot defend you
    static bool HandlePlayerbotDefendCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI and set defend mode (follow + protect)
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            botAI->Follow(player, 3.0f);  // Closer follow for defense
            botAI->SetAIState(Playerbot::BotAIState::FOLLOWING);
            handler->PSendSysMessage("Bot '%s' is now defending you.", botName.c_str());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot heal <name> - Make bot prioritize healing
    static bool HandlePlayerbotHealCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI - healing priority handled by class AI strategies
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            // Activate healing strategy if available
            botAI->ActivateStrategy("heal");
            handler->PSendSysMessage("Bot '%s' is now prioritizing healing (if class supports it).", botName.c_str());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot debug <name> - Toggle debug mode for bot
    static bool HandlePlayerbotDebugCommand(ChatHandler* handler, std::string botName)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Player* bot = ObjectAccessor::FindPlayerByName(botName);
        if (!bot)
        {
            handler->PSendSysMessage("Bot '%s' not found or not online.", botName.c_str());
            return true;
        }

        // Verify ownership
        uint32 accountId = player->GetSession()->GetAccountId();
        if (!bot->GetSession() || bot->GetSession()->GetAccountId() != accountId)
        {
            handler->SendSysMessage("You don't own this bot.");
            return true;
        }

        // Get bot AI and show debug info
        Playerbot::BotSession* botSession = dynamic_cast<Playerbot::BotSession*>(bot->GetSession());
        Playerbot::BotAI* botAI = botSession ? botSession->GetAI() : nullptr;

        if (botAI)
        {
            // Display debug information
            auto const& metrics = botAI->GetPerformanceMetrics();
            handler->PSendSysMessage("=== Debug Info: %s ===", botName.c_str());
            handler->PSendSysMessage("AI State: %u", static_cast<uint32>(botAI->GetAIState()));
            handler->PSendSysMessage("Total Updates: %u", metrics.totalUpdates.load());
            handler->PSendSysMessage("Actions Executed: %u", metrics.actionsExecuted.load());
            handler->PSendSysMessage("Triggers Processed: %u", metrics.triggersProcessed.load());
            handler->PSendSysMessage("Avg Update Time: %lld µs", metrics.averageUpdateTime.count());
        }
        else
        {
            handler->SendSysMessage("Bot AI is not active.");
        }

        return true;
    }

    // .bot spawn <class> <race> <name> - Create and spawn a new bot character
    static bool HandlePlayerbotSpawnCommand(ChatHandler* handler, uint8 classId, uint8 race, std::string name)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sPlayerbotConfig || !sPlayerbotConfig->GetBool("Playerbot.Enable", false))
        {
            handler->SendSysMessage("PlayerBot system is disabled in configuration.");
            return true;
        }

        uint32 accountId = player->GetSession()->GetAccountId();

        // Check bot count limits
        if (sPlayerbotConfig)
        {
            uint32 currentBots = Playerbot::sBotWorldSessionMgr->GetBotCountByAccount(accountId);
            uint32 maxPerAccount = sPlayerbotConfig->GetUInt("Playerbot.MaxBotsPerAccount", 10);

            if (currentBots >= maxPerAccount)
            {
                handler->PSendSysMessage("You have reached the maximum number of bots (%u).", maxPerAccount);
                return true;
            }
        }

        // Validate class and race IDs
        if (classId == 0 || classId > CLASS_EVOKER)
        {
            handler->PSendSysMessage("Invalid class ID %u. Valid range: 1-%u", classId, CLASS_EVOKER);
            return true;
        }

        // Note: Race IDs are not sequential (e.g., Dracthyr is 52/70). Validate against known maximums.
        if (race == 0 || race > RACE_DRACTHYR_HORDE)
        {
            handler->PSendSysMessage("Invalid race ID %u. Must be a valid playable race", race);
            return true;
        }

        // Default gender to male if not specified (can be enhanced later)
        uint8 gender = GENDER_MALE;

        // Create and spawn the bot
        ObjectGuid newCharGuid;
        bool success = Playerbot::sBotSpawner->CreateAndSpawnBot(
            accountId,
            classId,
            race,
            gender,
            name,
            newCharGuid);

        if (success)
        {
            handler->PSendSysMessage("Successfully created and spawned bot '%s' (GUID: %s)",
                name.c_str(), newCharGuid.ToString().c_str());
            handler->PSendSysMessage("Class: %s, Race: %s, Level: %u",
                GetClassName(classId),
                GetRaceName(race),
                Playerbot::BotCharacterCreator::GetStartingLevel(race, classId));
        }
        else
        {
            handler->PSendSysMessage("Failed to create bot '%s'. Check server logs for details.", name.c_str());
        }

        return true;
    }

    // Helper function - Get class name string
    static char const* GetClassName(uint8 classId)
    {
        switch (classId)
        {
            case CLASS_WARRIOR:       return "Warrior";
            case CLASS_PALADIN:       return "Paladin";
            case CLASS_HUNTER:        return "Hunter";
            case CLASS_ROGUE:         return "Rogue";
            case CLASS_PRIEST:        return "Priest";
            case CLASS_DEATH_KNIGHT:  return "Death Knight";
            case CLASS_SHAMAN:        return "Shaman";
            case CLASS_MAGE:          return "Mage";
            case CLASS_WARLOCK:       return "Warlock";
            case CLASS_MONK:          return "Monk";
            case CLASS_DRUID:         return "Druid";
            case CLASS_DEMON_HUNTER:  return "Demon Hunter";
            case CLASS_EVOKER:        return "Evoker";
            default:                  return "Unknown";
        }
    }

    // Helper function - Get race name string
    static char const* GetRaceName(uint8 raceId)
    {
        switch (raceId)
        {
            case RACE_HUMAN:              return "Human";
            case RACE_ORC:                return "Orc";
            case RACE_DWARF:              return "Dwarf";
            case RACE_NIGHTELF:           return "Night Elf";
            case RACE_UNDEAD_PLAYER:      return "Undead";
            case RACE_TAUREN:             return "Tauren";
            case RACE_GNOME:              return "Gnome";
            case RACE_TROLL:              return "Troll";
            case RACE_GOBLIN:             return "Goblin";
            case RACE_BLOODELF:           return "Blood Elf";
            case RACE_DRAENEI:            return "Draenei";
            case RACE_WORGEN:             return "Worgen";
            case RACE_PANDAREN_NEUTRAL:   return "Pandaren";
            case RACE_PANDAREN_ALLIANCE:  return "Pandaren (Alliance)";
            case RACE_PANDAREN_HORDE:     return "Pandaren (Horde)";
            case RACE_NIGHTBORNE:         return "Nightborne";
            case RACE_HIGHMOUNTAIN_TAUREN: return "Highmountain Tauren";
            case RACE_VOID_ELF:           return "Void Elf";
            case RACE_LIGHTFORGED_DRAENEI: return "Lightforged Draenei";
            case RACE_ZANDALARI_TROLL:    return "Zandalari Troll";
            case RACE_KUL_TIRAN:          return "Kul Tiran";
            case RACE_DARK_IRON_DWARF:    return "Dark Iron Dwarf";
            case RACE_VULPERA:            return "Vulpera";
            case RACE_MAGHAR_ORC:         return "Mag'har Orc";
            case RACE_MECHAGNOME:         return "Mechagnome";
            case RACE_DRACTHYR_HORDE:     return "Dracthyr (Horde)";
            case RACE_DRACTHYR_ALLIANCE:  return "Dracthyr (Alliance)";
            default:                      return "Unknown";
        }
    }

    // Helper function - Get power type name
    static char const* GetPowerName(Powers powerType)
    {
        switch (powerType)
        {
            case POWER_MANA:         return "Mana";
            case POWER_RAGE:         return "Rage";
            case POWER_FOCUS:        return "Focus";
            case POWER_ENERGY:       return "Energy";
            case POWER_COMBO_POINTS: return "Combo Points";
            case POWER_RUNES:        return "Runes";
            case POWER_RUNIC_POWER:  return "Runic Power";
            case POWER_SOUL_SHARDS:  return "Soul Shards";
            case POWER_LUNAR_POWER:  return "Astral Power";
            case POWER_HOLY_POWER:   return "Holy Power";
            case POWER_ALTERNATE_POWER: return "Alternate";
            case POWER_MAELSTROM:    return "Maelstrom";
            case POWER_CHI:          return "Chi";
            case POWER_INSANITY:     return "Insanity";
            case POWER_BURNING_EMBERS: return "Burning Embers";
            case POWER_DEMONIC_FURY: return "Demonic Fury";
            case POWER_ARCANE_CHARGES: return "Arcane Charges";
            case POWER_FURY:         return "Fury";
            case POWER_PAIN:         return "Pain";
            case POWER_ESSENCE:      return "Essence";
            default:                 return "Unknown";
        }
    }
};

// Script registration function
void AddSC_playerbot_commandscript()
{
    new playerbot_commandscript();
}
