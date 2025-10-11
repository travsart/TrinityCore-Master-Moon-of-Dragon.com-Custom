#include "ScriptMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "GameTime.h"
#include "World.h"
#include <unordered_map>
#include <vector>

using namespace Trinity::ChatCommands;

// Globale Buff-Daten
static std::unordered_map<ObjectGuid, uint32> BuffCooldown;
static std::vector<uint32> BuffStore;
static bool BuffsLoadedOnce = false;

class buff_commandscript : public CommandScript
{
public:
    buff_commandscript() : CommandScript("buff_commandscript") {}

    std::vector<Trinity::ChatCommands::ChatCommandBuilder> GetCommands() const override
    {
        static std::vector<Trinity::ChatCommands::ChatCommandBuilder> buffSubCommands;
        if (buffSubCommands.empty())
        {
            buffSubCommands.emplace_back("buff", HandleBuff, rbac::RBACPermissions::RBAC_ROLE_PLAYER, Trinity::ChatCommands::Console::No);
            buffSubCommands.emplace_back("reload", HandleReload, rbac::RBACPermissions::RBAC_ROLE_GAMEMASTER, Trinity::ChatCommands::Console::No);
        }

        return { Trinity::ChatCommands::ChatCommandBuilder("mod_buff", buffSubCommands) };
    }

    static bool HandleReload(ChatHandler* handler, const char* /*args*/)
    {
        LoadBuffsFromDB();

        if (BuffStore.empty())
        {
            handler->PSendSysMessage("No buffs found in the database.");
            return false;
        }

        handler->PSendSysMessage("Buff table reloaded. %u buffs loaded.", (uint32)BuffStore.size());
        return true;
    }

    static bool HandleBuff(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // Konfiguration prüfen
        if (!sConfigMgr->GetBoolDefault("BuffCommand.Enable", true))
        {
            handler->PSendSysMessage("The buff command is currently disabled.");
            return false;
        }

        int minLevel = sConfigMgr->GetIntDefault("BuffCommand.MinLevel", 80);
        if (player->GetLevel() < minLevel)
        {
            handler->PSendSysMessage("You must be at least level %d to use this command.", minLevel);
            return false;
        }

        // Bedingungen prüfen
        if (!player->IsAlive())
        {
            handler->PSendSysMessage("You cannot use this command while dead.");
            return false;
        }

        if (player->HasAuraType(SPELL_AURA_FEIGN_DEATH))
        {
            handler->PSendSysMessage("You cannot use this command while feigning death.");
            return false;
        }

        if (player->IsInCombat())
        {
            handler->PSendSysMessage("You cannot use this command while in combat.");
            return false;
        }

        if (player->HasStealthAura() || player->HasInvisibilityAura())
        {
            handler->PSendSysMessage("You cannot use this command while stealthed or invisible.");
            return false;
        }

        if (player->IsInFlight())
        {
            handler->PSendSysMessage("You cannot use this command while flying.");
            return false;
        }

        if (player->InBattleground() || player->GetMap()->IsBattleArena())
        {
            handler->PSendSysMessage("You cannot use this command in battlegrounds or arenas.");
            return false;
        }

        // Cooldown
        uint32 now = static_cast<uint32>(GameTime::GetGameTime());
        auto& cooldown = BuffCooldown[player->GetGUID()];
        uint32 cd = sConfigMgr->GetIntDefault("BuffCommand.Cooldown", 120);

        if (cooldown != 0 && now < cooldown)
        {
            handler->PSendSysMessage("You must wait %u seconds before using this command again.", cooldown - now);
            return false;
        }

        // Buff-Liste prüfen und ggf. laden
        if (BuffStore.empty())
        {
            if (!BuffsLoadedOnce)
            {
                LoadBuffsFromDB();
                BuffsLoadedOnce = true;
            }

            if (BuffStore.empty())
            {
                handler->PSendSysMessage("No buffs loaded. Use '.mod_buff reload' or check the database.");
                return false;
            }
        }

        // Buff anwenden
        for (uint32 spellId : BuffStore)
        {
            if (spellId == 0)
                continue;

            player->CastSpell(player, spellId, true);
        }

        BuffCooldown[player->GetGUID()] = now + cd;
        handler->PSendSysMessage("You have been buffed.");
        return true;
    }

    static void LoadBuffsFromDB()
    {
        BuffStore.clear();

        QueryResult result = WorldDatabase.Query("SELECT spell_id FROM player_buff ORDER BY spell_id ASC");
        if (!result)
        {
            TC_LOG_WARN("misc", "BuffCommand: No buffs found in 'player_buff' table.");
            return;
        }

        do
        {
            BuffStore.push_back((*result)[0].GetUInt32());
        } while (result->NextRow());

        TC_LOG_INFO("misc", "BuffCommand: Loaded %u buff spell IDs from database.", (uint32)BuffStore.size());
    }
};

class buff_worldscript : public WorldScript
{
public:
    buff_worldscript() : WorldScript("buff_worldscript") {}

    void OnStartup() override
    {
        TC_LOG_INFO("misc", "BuffCommand: Loading buffs on world startup...");
        buff_commandscript::LoadBuffsFromDB();
        BuffsLoadedOnce = true;
    }
};

class buff_playerscript : public PlayerScript
{
public:
    buff_playerscript() : PlayerScript("buff_playerscript") {}

    void OnLogin(Player* p, bool /*firstLogin*/) override
    {
        ChatHandler(p->GetSession()).PSendSysMessage(
            "This server is running |cff4CFF00Buff Mod |rUse '.mod_buff buff' to buff yourself."
        );
    }
};

void AddSC_MoDCore_buff_commandscript()
{
    new buff_commandscript();
    new buff_worldscript();
    new buff_playerscript();
}
