#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "RBAC.h"
#include "RoleplayDatabase.h"
#include "ScriptMgr.h"

#include <mutex>
#include <unordered_map>

namespace
{
    using namespace Trinity::ChatCommands;

    struct MorphData
    {
        uint32 displayId = 0;
        float scale = 1.0f;
    };

    std::unordered_map<uint64, MorphData> _morphCache;
    std::mutex _cacheMutex;

    void SaveMorphToDB(uint64 guid, MorphData const& data)
    {
        if (data.displayId == 0 && data.scale == 1.0f)
        {
            RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_DEL_PLAYER_MORPH);
            stmt->setUInt64(0, guid);
            RoleplayDatabase.Execute(stmt);
        }
        else
        {
            RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_REP_PLAYER_MORPH);
            stmt->setUInt64(0, guid);
            stmt->setUInt32(1, data.displayId);
            stmt->setFloat(2, data.scale);
            RoleplayDatabase.Execute(stmt);
        }
    }

    class PlayerMorphCommandScript : public CommandScript
    {
    public:
        PlayerMorphCommandScript() : CommandScript("player_morph_commands") { }

        std::span<ChatCommandBuilder const> GetCommands() const override
        {
            static ChatCommandTable commandTable =
            {
                { "wmorph",  rbac::RBAC_PERM_COMMAND_WMORPH,  false, HandleWMorphCommand,  "Syntax: .wmorph <displayId>\nMorphs your character to a display ID." },
                { "wscale",  rbac::RBAC_PERM_COMMAND_WSCALE,  false, HandleWScaleCommand,  "Syntax: .wscale <scale>\nScales your character model." },
                { "remorph", rbac::RBAC_PERM_COMMAND_REMORPH, false, HandleReMorphCommand, "Reapplies your current morph after a model reset." },
            };
            return commandTable;
        }

        static bool HandleWMorphCommand(ChatHandler* handler, char const* args)
        {
            Player* player = handler->GetSession()->GetPlayer();
            if (!player)
                return false;

            uint32 displayId = 0;
            if (args && *args)
                displayId = static_cast<uint32>(atoi(args));

            uint64 guid = player->GetGUID().GetCounter();

            if (displayId == 0)
            {
                player->DeMorph();
                handler->PSendSysMessage("Morph removed.");
            }
            else
            {
                player->SetDisplayId(displayId);
                handler->PSendSysMessage("Morphed to display ID %u.", displayId);
            }

            std::lock_guard<std::mutex> lock(_cacheMutex);
            _morphCache[guid].displayId = displayId;
            SaveMorphToDB(guid, _morphCache[guid]);
            return true;
        }

        static bool HandleWScaleCommand(ChatHandler* handler, char const* args)
        {
            Player* player = handler->GetSession()->GetPlayer();
            if (!player)
                return false;

            float scale = 1.0f;
            if (args && *args)
                scale = static_cast<float>(atof(args));

            // Clamp to valid range
            if (scale < 0.1f)
                scale = 0.1f;
            else if (scale > 10.0f)
                scale = 10.0f;

            uint64 guid = player->GetGUID().GetCounter();

            player->SetObjectScale(scale);

            if (scale == 1.0f)
                handler->PSendSysMessage("Scale reset to default.");
            else
                handler->PSendSysMessage("Scale set to %.2f.", scale);

            std::lock_guard<std::mutex> lock(_cacheMutex);
            _morphCache[guid].scale = scale;
            SaveMorphToDB(guid, _morphCache[guid]);
            return true;
        }

        static bool HandleReMorphCommand(ChatHandler* handler, char const* /*args*/)
        {
            Player* player = handler->GetSession()->GetPlayer();
            if (!player)
                return false;

            uint64 guid = player->GetGUID().GetCounter();

            std::lock_guard<std::mutex> lock(_cacheMutex);
            auto it = _morphCache.find(guid);
            if (it == _morphCache.end() || (it->second.displayId == 0 && it->second.scale == 1.0f))
            {
                handler->PSendSysMessage("No morph data to restore.");
                return true;
            }

            MorphData const& data = it->second;
            if (data.displayId > 0)
                player->SetDisplayId(data.displayId);
            if (data.scale != 1.0f)
                player->SetObjectScale(data.scale);

            handler->PSendSysMessage("Morph restored.");
            return true;
        }
    };

    class PlayerMorphPlayerScript : public PlayerScript
    {
    public:
        PlayerMorphPlayerScript() : PlayerScript("player_morph_login") { }

        void OnLogin(Player* player, bool /*firstLogin*/) override
        {
            uint64 guid = player->GetGUID().GetCounter();

            RoleplayDatabasePreparedStatement* stmt = RoleplayDatabase.GetPreparedStatement(Roleplay_SEL_PLAYER_MORPH);
            stmt->setUInt64(0, guid);
            PreparedQueryResult result = RoleplayDatabase.Query(stmt);

            if (!result)
                return;

            Field* fields = result->Fetch();
            uint32 displayId = fields[0].GetUInt32();
            float scale = fields[1].GetFloat();

            {
                std::lock_guard<std::mutex> lock(_cacheMutex);
                _morphCache[guid] = { displayId, scale };
            }

            if (displayId > 0)
                player->SetDisplayId(displayId);
            if (scale != 1.0f)
                player->SetObjectScale(scale);
        }
    };
}

void AddSC_PlayerMorphScripts()
{
    new PlayerMorphCommandScript();
    new PlayerMorphPlayerScript();
}
