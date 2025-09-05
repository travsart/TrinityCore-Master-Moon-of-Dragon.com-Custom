#include "ScriptMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "World.h"
#include <unordered_map>
#include <string>
#include "ChatCommand.h"

using namespace Trinity::ChatCommands;

static bool g_GlobalChatEnabled = true;
static bool g_GlobalChatAnnounce = true;

struct ChatState { bool enabled = true; };
static std::unordered_map<ObjectGuid, ChatState> ChatStates;

static std::string GetClassIconColor(Player* player)
{
    switch (player->GetClass())
    {
    case CLASS_DEATH_KNIGHT: return "|cffC41F3B|TInterface\\icons\\Spell_Deathknight_ClassIcon:15|t";
    case CLASS_DRUID:        return "|cffFF7D0A|TInterface\\icons\\Ability_Druid_Maul:15|t";
    case CLASS_HUNTER:       return "|cffABD473|TInterface\\icons\\INV_Weapon_Bow_07:15|t";
    case CLASS_MAGE:         return "|cff69CCF0|TInterface\\icons\\INV_Staff_13:15|t";
    case CLASS_PALADIN:      return "|cffF58CBA|TInterface\\icons\\INV_Hammer_01:15|t";
    case CLASS_PRIEST:       return "|cffFFFFFF|TInterface\\icons\\INV_Staff_30:15|t";
    case CLASS_ROGUE:        return "|cffFFF569|TInterface\\icons\\INV_ThrowingKnife_04:15|t";
    case CLASS_SHAMAN:       return "|cff0070DE|TInterface\\icons\\Spell_Nature_BloodLust:15|t";
    case CLASS_WARLOCK:      return "|cff9482C9|TInterface\\icons\\Spell_Nature_FaerieFire:15|t";
    case CLASS_WARRIOR:      return "|cffC79C6E|TInterface\\icons\\INV_Sword_27.png:15|t";
    case CLASS_MONK:         return "|cffC79C6E|TInterface\\icons\\monk_ability_transcendence:15|t";
    case CLASS_DEMON_HUNTER: return "|cffC79C6E|TInterface\\icons\\ability_demonhunter_blur:15|t";
    case CLASS_EVOKER:       return "|cffC79C6E|TInterface\\icons\\ability_evoker_blackattunement:15|t";
    default:                 return "|cffFFFFFF";
    }
}

static std::string FormatGlobalMsg(Player* player, std::string_view text)
{
    std::string msg = "|cff00ff00[Global] ";
    msg += (player->GetTeamId() == TEAM_ALLIANCE) ? "|TInterface\\PVPFrame\\PVP-Currency-Alliance:15|t " : "|TInterface\\PVPFrame\\PVP-Currency-Horde:15|t ";
    msg += GetClassIconColor(player) + " |Hplayer:" + player->GetName() + "|h[" + player->GetName() + "]|h|r: ";
    msg += "|cffFFFF00" + std::string(text);
    return msg;
}

static void BroadcastGlobal(const std::string& msg)
{
    for (const auto& sessionPair : sWorld->GetAllSessions())
    {
        WorldSession* session = sessionPair.second;
        if (!session)
            continue;

        Player* player = session->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        auto it = ChatStates.find(player->GetGUID());
        if (it != ChatStates.end() && !it->second.enabled)
            continue;

        ChatHandler(session).SendSysMessage(msg.c_str());
    }
}

class global_chat_commandscript : public CommandScript
{
public:
    global_chat_commandscript() : CommandScript("global_chat_commandscript") {}

    static bool HandleOn(ChatHandler* handler, const char* /*args*/)
    {
        if (!g_GlobalChatEnabled)
            handler->SendSysMessage("Global Chat is disabled.");
        else
        {
            auto guid = handler->GetSession()->GetPlayer()->GetGUID();
            ChatStates[guid].enabled = true;
            handler->SendSysMessage("Global Chat enabled for you.");
        }
        return true;
    }

    static bool HandleOff(ChatHandler* handler, const char* /*args*/)
    {
        if (!g_GlobalChatEnabled)
            handler->SendSysMessage("Global Chat is disabled.");
        else
        {
            auto guid = handler->GetSession()->GetPlayer()->GetGUID();
            ChatStates[guid].enabled = false;
            handler->SendSysMessage("Global Chat disabled for you.");
        }
        return true;
    }

    static bool HandleChat(ChatHandler* handler, const char* args)
    {
        Player* p = handler->GetSession()->GetPlayer();
        if (!p)
            return false;

        if (!g_GlobalChatEnabled)
        {
            handler->SendSysMessage("Global Chat is turned off globally.");
            return true;
        }

        auto it = ChatStates.find(p->GetGUID());
        if (it != ChatStates.end() && !it->second.enabled)
        {
            handler->SendSysMessage("Global Chat is turned off for you. Use '.chat on'.");
            return true;
        }

        if (!args || !*args)
        {
            handler->SendSysMessage("Usage: .chat <message>");
            return true;
        }

        std::string msg = FormatGlobalMsg(p, args);
        BroadcastGlobal(msg);
        return true;
    }
    std::vector<Trinity::ChatCommands::ChatCommandBuilder> GetCommands() const override
    {
        static std::vector<Trinity::ChatCommands::ChatCommandBuilder> chatSubCommands;
        if (chatSubCommands.empty())
        {
            chatSubCommands.emplace_back("on", HandleOn, rbac::RBACPermissions::RBAC_ROLE_PLAYER, Trinity::ChatCommands::Console::No);
            chatSubCommands.emplace_back("off", HandleOff, rbac::RBACPermissions::RBAC_ROLE_PLAYER, Trinity::ChatCommands::Console::No);
            chatSubCommands.emplace_back("", HandleChat, rbac::RBACPermissions::RBAC_ROLE_PLAYER, Trinity::ChatCommands::Console::No);
        }

        return { Trinity::ChatCommands::ChatCommandBuilder("chat", chatSubCommands) };
    }

};


class global_chat_playerscript : public PlayerScript
{
public:
    global_chat_playerscript() : PlayerScript("global_chat_playerscript") {}

    void OnLogin(Player* p, bool /*firstLogin*/) override
    {
        if (!ChatStates.contains(p->GetGUID()))
            ChatStates[p->GetGUID()].enabled = true;

        if (g_GlobalChatEnabled && g_GlobalChatAnnounce)
            ChatHandler(p->GetSession()).SendSysMessage(
                "This server is running |cff4CFF00Global Chat |rUse '.chat' to speak globally."
            );
    }
};

class global_chat_worldscript : public WorldScript
{
public:
    global_chat_worldscript() : WorldScript("global_chat_worldscript") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        g_GlobalChatEnabled = sConfigMgr->GetBoolDefault("GlobalChat.Enable", true);
        g_GlobalChatAnnounce = sConfigMgr->GetBoolDefault("GlobalChat.Announce", true);
    }
};

void AddSC_MoDCore_global_chat()
{
    new global_chat_commandscript();
    new global_chat_playerscript();
    new global_chat_worldscript();
}
