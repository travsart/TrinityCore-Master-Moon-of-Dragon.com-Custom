/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Chat.h"
#include "ChatCommandTags.h"
#include "ChatCommand.h"
#include "DB2Stores.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "MiscPackets.h"
#include "MovementGenerator.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "Util.h"
#include "WorldSession.h"
#include "RBAC.h"
#include "MotionMaster.h"
#include "Map.h"
#include "World.h"
#include "WorldPacket.h"
#include "GameTime.h"
#include "RoleplayDatabase.h"

using namespace Trinity::ChatCommands;
#include "RolePlay.h"

class StaticTimeManager
{
private:
    static inline uint32 m_staticHour = 12;
    static inline uint32 m_staticMinute = 0;
    static inline uint32 m_staticYear = 1900;
    static inline uint32 m_staticMonth = 1;
    static inline uint32 m_staticMonthDay = 1;

    static inline bool m_timeFreezed = false;
    static inline bool m_timeTransitioning = false;

    static inline uint32 m_targetHour = 12;
    static inline uint32 m_targetMinute = 0;

    static inline uint32 m_transitionStepMs = 1000;
    static inline uint32 m_transitionTimer = 0;

public:

    static void SaveStaticTimeToDB()
    {
        using namespace std::string_view_literals;

        RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_DEL_SERVER_SETTINGS);
        RoleplayDatabase.Execute(stmt);

        stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_SERVER_SETTINGS);
        stmt->setString(0, "static_hour"sv);
        stmt->setString(1, std::to_string(m_staticHour));
        RoleplayDatabase.Execute(stmt);

        stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_SERVER_SETTINGS);
        stmt->setString(0, "static_minute"sv);
        stmt->setString(1, std::to_string(m_staticMinute));
        RoleplayDatabase.Execute(stmt);

        stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_SERVER_SETTINGS);
        stmt->setString(0, "time_freezed"sv);
        stmt->setString(1, std::to_string(m_timeFreezed ? 1 : 0));
        RoleplayDatabase.Execute(stmt);
    }

    static void LoadStaticTimeFromDB()
    {
        RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_SEL_SERVER_SETTINGS);
        PreparedQueryResult result = RoleplayDatabase.Query(stmt);

        if (!result)
        {
            ResetToServerTime();
            return;
        }

        bool hourSet = false, minuteSet = false, freezeSet = false;

        do
        {
            Field* fields = result->Fetch();
            std::string settingName = fields[0].GetString();
            std::string settingValue = fields[1].GetString();

            if (settingName == "static_hour")
            {
                m_staticHour = Trinity::StringTo<int>(settingValue).value_or(0);
                hourSet = true;
            }
            else if (settingName == "static_minute")
            {
                m_staticMinute = Trinity::StringTo<int>(settingValue).value_or(0);
                minuteSet = true;
            }
            else if (settingName == "time_freezed")
            {
                m_timeFreezed = (settingValue == "1");
                freezeSet = true;
            }

        } while (result->NextRow());

        if (!hourSet || !minuteSet || !freezeSet)
        {
            ResetToServerTime();
        }

        SendTimeSync();
    }

    static bool IsTimeFreezed()
    {
        return m_timeFreezed && !m_timeTransitioning;
    }

    static void SetStaticTimeSmooth(uint32 targetHour, uint32 targetMinute, uint32 stepMs = 1000)
    {
        if (targetHour > 23 || targetMinute > 59)
            return;

        m_targetHour = targetHour;
        m_targetMinute = targetMinute;
        m_transitionStepMs = stepMs;
        m_transitionTimer = 0;
        m_timeTransitioning = true;
        m_timeFreezed = false;

    }

    static void SetStaticTime(uint32 hour, uint32 minute, bool freeze = false)
    {
        m_staticHour = hour;
        m_staticMinute = minute;
        m_timeFreezed = freeze;
        m_timeTransitioning = false;

        SaveStaticTimeToDB();
        SendTimeSync();
    }

    static void ResetToServerTime()
    {
        time_t now = time(nullptr);
        struct tm* localTime = localtime(&now);

        m_staticHour = localTime->tm_hour;
        m_staticMinute = localTime->tm_min;
        m_timeFreezed = false;
        m_timeTransitioning = false;

        RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_DEL_SERVER_SETTINGS);
        RoleplayDatabase.Execute(stmt);

        SendTimeSync();
    }

    static WorldPacket const* BuildTimeSyncPacket()
    {
        static WorldPackets::Misc::LoginSetTimeSpeed timePacket;
        WowTime custom;

        time_t nowYM = time(nullptr);
        struct tm* localTimeYM = localtime(&nowYM);

        m_staticYear = (localTimeYM->tm_year + 1900) % 100;
        m_staticMonth = localTimeYM->tm_mon;
        m_staticMonthDay = localTimeYM->tm_mday;

        custom.SetHour(m_staticHour);
        custom.SetMinute(m_staticMinute);
        custom.SetYear(m_staticYear);
        custom.SetMonth(m_staticMonth);
        custom.SetMonthDay(m_staticMonthDay);
        timePacket.GameTime = custom;
        timePacket.ServerTime = custom;
        static float const TimeSpeed = 0.01666667f;
        timePacket.NewSpeed = TimeSpeed;

        return timePacket.Write();
    }

    static void SendTimeSync()
    {
        sWorld->SendGlobalMessage(BuildTimeSyncPacket());
    }

    static void SendTimeSyncToPlayer(Player* player)
    {
        if (player)
            player->SendDirectMessage(BuildTimeSyncPacket());
    }

    static void Update(uint32 diff)
    {
        if (!m_timeTransitioning)
            return;

        m_transitionTimer += diff;

        if (m_transitionTimer >= m_transitionStepMs)
        {
            m_transitionTimer = 0;

            m_staticMinute++;
            if (m_staticMinute >= 60)
            {
                m_staticMinute = 0;
                m_staticHour++;
                if (m_staticHour >= 24)
                    m_staticHour = 0;
            }

            if (m_staticHour == m_targetHour && m_staticMinute == m_targetMinute)
            {
                m_timeTransitioning = false;
                m_timeFreezed = true;
                SaveStaticTimeToDB();
            }

            SendTimeSync();
        }
    }
};

using namespace Trinity::ChatCommands;
class free_share_scripts : public CommandScript
{
public:
    free_share_scripts() : CommandScript("free_share_scripts") {}

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable typingCommandTable =
        {
            { "on",              HandleTypingOnCommand,     rbac::RBAC_PERM_COMMAND_TYPING_ON,     Console::No },
            { "off",             HandleTypingOffCommand,    rbac::RBAC_PERM_COMMAND_TYPING_OFF,    Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "barbershop",      HandleBarberCommand,       rbac::RBAC_PERM_COMMAND_BARBER,        Console::No },
            { "castgroup",       HandleCastGroupCommand,    rbac::RBAC_PERM_COMMAND_CAST_GROUP,    Console::No },
            { "castgroupscene",  HandleCastSceneCommand,    rbac::RBAC_PERM_COMMAND_CAST_SCENE,    Console::No },
            { "npcmoveto",       HandleNpcMoveTo,           rbac::RBAC_PERM_COMMAND_NPC_MOVE,      Console::No },
            { "npcguidsay",      HandleNpcGuidSay,          rbac::RBAC_PERM_COMMAND_NPC_SAY,       Console::No },
            { "npcguidyell",     HandleNpcGuidYell,         rbac::RBAC_PERM_COMMAND_NPC_YELL,      Console::No },
            { "settime",         HandleSetTimeCommand,      rbac::RBAC_PERM_COMMAND_SETTIME,       Console::No },
            { "typing",          typingCommandTable },
        };

        return commandTable;
    }

    // custom command .typing on
    static bool HandleTypingOnCommand(ChatHandler* handler)
    {
        Player* plr = handler->GetSession()->GetPlayer();

        if (!plr)
        {
            handler->SendSysMessage("You must be in-game to use this command.");
            return false;
        }

        plr->CastSpell(plr, 156354, false);

        return true;
    }

    // custom command .typing off
    static bool HandleTypingOffCommand(ChatHandler* handler)
    {
        Player* plr = handler->GetSession()->GetPlayer();

        if (!plr)
        {
            handler->SendSysMessage("You must be in-game to use this command.");
            return false;
        }

        if (plr->HasAura(156354))
            plr->RemoveAura(156354);

        return true;
    }

    // custom command .barber
    static bool HandleBarberCommand(ChatHandler* handler, Optional<uint32> featureMask)
    {
        if (!featureMask) {
            featureMask = 0;
        }

        if (WorldSession* session = handler->GetSession())
        {
            WorldPackets::Misc::EnableBarberShop enableBarberShop;
            enableBarberShop.CustomizationFeatureMask = *featureMask;
            session->GetPlayer()->SendDirectMessage(enableBarberShop.Write());
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    // custom command .castgroup
    static bool HandleCastGroupCommand(ChatHandler* handler, SpellInfo const* spellInfo)
    {
        uint32 spellId = spellInfo->Id;

        if (!spellId)
        {
            handler->SendSysMessage("Invalid spell ID.");
            return false;
        }

        if (!handler->GetSession()->GetPlayer()->GetGroup())
        {
            handler->SendSysMessage("You must be in a group to use this command.");
            return false;
        }

        for (GroupReference const& itr : handler->GetSession()->GetPlayer()->GetGroup()->GetMembers())
        {
            Player* plr = itr.GetSource();
            if (!plr || !plr->GetSession())
                continue;

            plr->CastSpell(plr, spellId, false);
        }

        return true;
    }

    // custom command .castscene
    static bool HandleCastSceneCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("Usage: .castscene <scenePackageId> [flags]");
            return false;
        }

        std::vector<std::string_view> tokens = Trinity::Tokenize(args, ' ', false);
        if (tokens.empty())
        {
            handler->SendSysMessage("Usage: .castscene <scenePackageId> [flags]");
            return false;
        }

        uint32 scenePackageId = Trinity::StringTo<uint32>(tokens[0]).value_or(0);
        uint32 flags = tokens.size() > 1 ? Trinity::StringTo<uint32>(tokens[1]).value_or(0) : 0;

        if (!sSceneScriptPackageStore.HasRecord(scenePackageId))
        {
            handler->SendSysMessage("Scene package not found.");
            return false;
        }

        if (!handler->GetSession()->GetPlayer()->GetGroup())
            return false;

        for (GroupReference const& itr : handler->GetSession()->GetPlayer()->GetGroup()->GetMembers())
        {
            Player* plr = itr.GetSource();
            if (!plr || !plr->GetSession())
                continue;

            plr->GetSceneMgr().PlaySceneByPackageId(scenePackageId, SceneFlag(flags));
        }

        return true;
    }

    // custom command .npcmoveto
    static bool HandleNpcMoveTo(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("Usage: .npcmoveto <guid> <x> <y> <z>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        std::vector<std::string_view> tokens = Trinity::Tokenize(args, ' ', false);
        if (tokens.size() < 4)
        {
            handler->SendSysMessage("Usage: .npcmoveto <guid> <x> <y> <z>");
            return false;
        }

        std::string guidStr(tokens[0]);
        float x2 = Trinity::StringTo<float>(tokens[1]).value_or(0.0f);
        float y2 = Trinity::StringTo<float>(tokens[2]).value_or(0.0f);
        float z2 = Trinity::StringTo<float>(tokens[3]).value_or(0.0f);

        char* cId = handler->extractKeyFromLink(guidStr.data(), "Hcreature");
        if (!cId)
            return false;

        ObjectGuid::LowType lowguid = Trinity::StringTo<ObjectGuid::LowType>(cId).value_or(UI64LIT(0));

        Creature* creature = handler->GetCreatureFromPlayerMapByDbGuid(lowguid);

        if (!creature)
        {
            handler->SendSysMessage("Creature not found with that GUID.");
            return false;
        }

        if (player->GetMapId() != creature->GetMapId())
        {
            handler->SendSysMessage("Creature is not on your current map.");
            return false;
        }

        creature->GetMotionMaster()->MovePoint(0, x2, y2, z2);

        return true;
    }

    // custom command .npcguidsay
    static bool HandleNpcGuidSay(ChatHandler* handler, char const* args)
    {
        std::string_view argsView(args);
        auto spacePos = argsView.find(' ');
        if (spacePos == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .npcguidsay <guid> <text>");
            return false;
        }

        std::string guidStr(argsView.substr(0, spacePos));
        std::string text(argsView.substr(spacePos + 1));

        if (text.empty())
        {
            handler->SendSysMessage("Usage: .npcguidsay <guid> <text>");
            return false;
        }

        char* cId = handler->extractKeyFromLink(guidStr.data(), "Hcreature");
        if (!cId)
            return false;

        ObjectGuid::LowType lowguid = Trinity::StringTo<ObjectGuid::LowType>(cId).value_or(UI64LIT(0));

        Creature* creature = handler->GetCreatureFromPlayerMapByDbGuid(lowguid);

        if (!creature)
        {
            handler->SendSysMessage("Creature not found with that GUID.");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        if (player->GetMapId() != creature->GetMapId())
        {
            handler->SendSysMessage("Creature is not on your current map.");
            return false;
        }

        creature->Say(text, LANG_UNIVERSAL);

        char lastchar = text.back();
        switch (lastchar)
        {
        case '?':   creature->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);      break;
        case '!':   creature->HandleEmoteCommand(EMOTE_ONESHOT_EXCLAMATION);   break;
        default:    creature->HandleEmoteCommand(EMOTE_ONESHOT_TALK);          break;
        }

        return true;
    }

    // custom command .npcguidyell
    static bool HandleNpcGuidYell(ChatHandler* handler, char const* args)
    {
        std::string_view argsView(args);
        auto spacePos = argsView.find(' ');
        if (spacePos == std::string_view::npos)
        {
            handler->SendSysMessage("Usage: .npcguidyell <guid> <text>");
            return false;
        }

        std::string guidStr(argsView.substr(0, spacePos));
        std::string text(argsView.substr(spacePos + 1));

        if (text.empty())
        {
            handler->SendSysMessage("Usage: .npcguidyell <guid> <text>");
            return false;
        }

        char* cId = handler->extractKeyFromLink(guidStr.data(), "Hcreature");
        if (!cId)
            return false;

        ObjectGuid::LowType lowguid = Trinity::StringTo<ObjectGuid::LowType>(cId).value_or(UI64LIT(0));

        Creature* creature = handler->GetCreatureFromPlayerMapByDbGuid(lowguid);

        if (!creature)
        {
            handler->SendSysMessage("Creature not found with that GUID.");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        if (player->GetMapId() != creature->GetMapId())
        {
            handler->SendSysMessage("Creature is not on your current map.");
            return false;
        }

        creature->Yell(text, LANG_UNIVERSAL);
        creature->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);

        return true;
    }

    // custom command .settime
    static bool HandleSetTimeCommand(ChatHandler* handler, Optional<uint32> hour, Optional<uint32> minute, Optional<std::string> mode, Optional<uint32> speedMs)
    {
        if (hour && *hour == 999)
        {
            StaticTimeManager::ResetToServerTime();
            handler->PSendSysMessage("Time reset to server time.");
            return true;
        }

        uint32 setHour = hour && *hour >= 0 ? *hour : 15;
        uint32 setMinute = minute && *minute >= 0 ? *minute : 30;

        if (setHour > 23 || setMinute > 59)
        {
            handler->SendSysMessage("Incorrect time. Use hours 0-23, minutes 0-59.");
            return false;
        }

        std::string modeStr = mode ? *mode : "instant";

        if (modeStr == "smooth")
        {
            uint32 stepMs = speedMs ? *speedMs : 1000;
            StaticTimeManager::SetStaticTimeSmooth(setHour, setMinute, stepMs);
            handler->PSendSysMessage("Smooth time transition started to %02u:%02u.", setHour, setMinute);
        }
        else
        {
            StaticTimeManager::SetStaticTime(setHour, setMinute, true);
            handler->PSendSysMessage("Server time is set to %02u:%02u (frozen).", setHour, setMinute);
        }

        return true;
    }
};

class PlayerScript_TimeSync : public PlayerScript
{
public:
    PlayerScript_TimeSync() : PlayerScript("PlayerScript_TimeSync") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        StaticTimeManager::SendTimeSyncToPlayer(player);
    }

    void OnLogout(Player* player) override
    {
        sRoleplay->RemovePlayerExtraData(player->GetGUID().GetCounter());
    }
};

class WorldScript_TimeSync : public WorldScript
{
public:
    WorldScript_TimeSync() : WorldScript("WorldScript_TimeSync") {}

    void OnStartup() override
    {
        StaticTimeManager::LoadStaticTimeFromDB();
    }

    void OnUpdate(uint32 diff) override
    {
        StaticTimeManager::Update(diff);

        static uint32 timeCheckTimer = 0;
        timeCheckTimer += diff;

        if (timeCheckTimer >= 30 * IN_MILLISECONDS)
        {
            if (StaticTimeManager::IsTimeFreezed())
            {
                StaticTimeManager::SendTimeSync();
            }

            timeCheckTimer = 0;
        }
    }
};

void AddSC_free_share_scripts()
{
    new free_share_scripts();
    new PlayerScript_TimeSync();
    new WorldScript_TimeSync();
}
