#include "CompanionMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ChatCommandTags.h"
#include "Player.h"
#include "RBAC.h"

#include <stdexcept>

using namespace Trinity::ChatCommands;

class CompanionCommands : public CommandScript
{
public:
    CompanionCommands() : CommandScript("companion_commands") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable compCommandTable =
        {
            { "roster",   HandleCompRoster,  rbac::RBAC_ROLE_PLAYER, Console::No },
            { "set",      HandleCompSet,     rbac::RBAC_ROLE_PLAYER, Console::No },
            { "clear",    HandleCompClear,   rbac::RBAC_ROLE_PLAYER, Console::No },
            { "summon",   HandleCompSummon,  rbac::RBAC_ROLE_PLAYER, Console::No },
            { "dismiss",  HandleCompDismiss, rbac::RBAC_ROLE_PLAYER, Console::No },
            { "mode",     HandleCompMode,    rbac::RBAC_ROLE_PLAYER, Console::No },
            { "follow",   HandleCompFollow,  rbac::RBAC_ROLE_PLAYER, Console::No },
            { "status",   HandleCompStatus,  rbac::RBAC_ROLE_PLAYER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "comp", compCommandTable },
        };

        return commandTable;
    }

    // .comp roster — list all available companions
    static bool HandleCompRoster(ChatHandler* handler)
    {
        auto const& roster = sCompanionMgr->GetRoster();
        if (roster.empty())
        {
            handler->SendSysMessage("No companions available in the roster.");
            return true;
        }

        handler->PSendSysMessage("Available companions (%zu):", roster.size());
        for (auto const& entry : roster)
        {
            handler->PSendSysMessage("  [%u] %s — %s", entry.entry, entry.name.c_str(),
                Companion::RoleToString(entry.role));
        }
        return true;
    }

    // .comp set <slot 1-5> <name or entry>
    static bool HandleCompSet(ChatHandler* handler, uint32 slot, Tail nameOrEntry)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (slot < 1 || slot > Companion::MAX_SQUAD_SLOTS)
        {
            handler->PSendSysMessage("Slot must be 1-%u.", Companion::MAX_SQUAD_SLOTS);
            return false;
        }

        std::string arg(nameOrEntry);
        if (arg.empty())
        {
            handler->SendSysMessage("Usage: .comp set <slot 1-5> <name or entry>");
            return false;
        }

        // Try numeric entry first
        uint32 rosterEntry = 0;
        try { rosterEntry = std::stoul(arg); } catch (std::exception const&) { rosterEntry = 0; }

        if (rosterEntry == 0)
        {
            // Search by name (case-insensitive prefix)
            std::string lower = arg;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            for (auto const& entry : sCompanionMgr->GetRoster())
            {
                std::string nameLower = entry.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(lower) == 0)
                {
                    rosterEntry = entry.entry;
                    break;
                }
            }
        }

        if (rosterEntry == 0)
        {
            handler->PSendSysMessage("Companion '%s' not found in roster.", arg.c_str());
            return false;
        }

        Companion::RosterEntry const* roster = sCompanionMgr->GetRosterEntry(rosterEntry);
        if (!roster)
        {
            handler->PSendSysMessage("Companion entry %u not found in roster.", rosterEntry);
            return false;
        }

        uint8 slotIdx = uint8(slot - 1);
        if (sCompanionMgr->SetSquadSlot(player, slotIdx, rosterEntry))
            handler->PSendSysMessage("Slot %u: %s (%s)", slot, roster->name.c_str(), Companion::RoleToString(roster->role));
        else
            handler->SendSysMessage("Failed to set companion slot.");

        return true;
    }

    // .comp clear <slot 1-5 | all>
    static bool HandleCompClear(ChatHandler* handler, Tail arg)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::string str(arg);
        if (str.empty())
        {
            handler->SendSysMessage("Usage: .comp clear <slot 1-5 | all>");
            return false;
        }

        if (str == "all")
        {
            // Dismiss if summoned
            Companion::PlayerSquadState* state = sCompanionMgr->GetPlayerState(player->GetGUID().GetCounter());
            if (state && state->summoned)
                sCompanionMgr->DismissSquad(player);

            sCompanionMgr->ClearAllSlots(player);
            handler->SendSysMessage("All companion slots cleared.");
            return true;
        }

        uint32 slot = 0;
        try { slot = std::stoul(str); } catch (std::exception const&) { slot = 0; }

        if (slot < 1 || slot > Companion::MAX_SQUAD_SLOTS)
        {
            handler->PSendSysMessage("Slot must be 1-%u, or 'all'.", Companion::MAX_SQUAD_SLOTS);
            return false;
        }

        sCompanionMgr->ClearSquadSlot(player, uint8(slot - 1));
        handler->PSendSysMessage("Slot %u cleared.", slot);
        return true;
    }

    // .comp summon
    static bool HandleCompSummon(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Companion::PlayerSquadState* state = sCompanionMgr->GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
        {
            handler->SendSysMessage("No squad data loaded.");
            return false;
        }

        // Check at least one slot is set
        bool hasSlot = false;
        for (uint8 i = 0; i < Companion::MAX_SQUAD_SLOTS; ++i)
            if (state->squad[i].rosterEntry) { hasSlot = true; break; }

        if (!hasSlot)
        {
            handler->SendSysMessage("No companions assigned. Use .comp set <slot> <name> first.");
            return false;
        }

        sCompanionMgr->SummonSquad(player);
        handler->PSendSysMessage("Squad summoned (%zu companions).", state->active.size());
        return true;
    }

    // .comp dismiss
    static bool HandleCompDismiss(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        sCompanionMgr->DismissSquad(player);
        handler->SendSysMessage("Squad dismissed.");
        return true;
    }

    // .comp mode <passive|defend|assist>
    static bool HandleCompMode(ChatHandler* handler, Tail arg)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::string modeStr(arg);
        if (modeStr.empty())
        {
            handler->SendSysMessage("Usage: .comp mode <passive|defend|assist>");
            return false;
        }

        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

        Companion::Mode mode;
        if (modeStr == "passive")      mode = Companion::MODE_PASSIVE;
        else if (modeStr == "defend")  mode = Companion::MODE_DEFEND;
        else if (modeStr == "assist")  mode = Companion::MODE_ASSIST;
        else
        {
            handler->SendSysMessage("Valid modes: passive, defend, assist");
            return false;
        }

        sCompanionMgr->SetMode(player, mode);
        handler->PSendSysMessage("Companion mode set to: %s", Companion::ModeToString(mode));
        return true;
    }

    // .comp follow — toggle follow on/off
    static bool HandleCompFollow(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Companion::PlayerSquadState* state = sCompanionMgr->GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
        {
            handler->SendSysMessage("No squad data loaded.");
            return false;
        }

        bool newFollow = !state->control.following;
        sCompanionMgr->SetFollowing(player, newFollow);
        handler->PSendSysMessage("Following: %s", newFollow ? "ON" : "OFF");
        return true;
    }

    // .comp status (also default for bare .comp)
    static bool HandleCompStatus(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Companion::PlayerSquadState* state = sCompanionMgr->GetPlayerState(player->GetGUID().GetCounter());
        if (!state)
        {
            handler->SendSysMessage("No squad data loaded. Try logging out and back in.");
            return false;
        }

        handler->PSendSysMessage("Mode: %s | Following: %s | Summoned: %s",
            Companion::ModeToString(state->control.mode),
            state->control.following ? "Yes" : "No",
            state->summoned ? "Yes" : "No");

        for (uint8 i = 0; i < Companion::MAX_SQUAD_SLOTS; ++i)
        {
            if (state->squad[i].rosterEntry)
            {
                handler->PSendSysMessage("  Slot %u: %s (%s)", i + 1,
                    state->squad[i].rosterEntry->name.c_str(),
                    Companion::RoleToString(state->squad[i].rosterEntry->role));
            }
            else
            {
                handler->PSendSysMessage("  Slot %u: [empty]", i + 1);
            }
        }

        return true;
    }
};

void AddSC_CompanionCommands()
{
    new CompanionCommands();
}
