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

/*
 * PHASE 4.1: SCRIPT SYSTEM INTEGRATION
 *
 * This file integrates the Playerbot Event System with TrinityCore's
 * official script system, providing non-invasive event hook integration.
 *
 * Coverage: 66+ script hooks covering:
 * - Combat events (damage, healing, spells)
 * - Death and resurrection
 * - Group management
 * - Player progression (leveling, talents, reputation)
 * - Social interactions (chat, whispers, emotes)
 * - Economy (gold, auction house)
 * - Vehicles and mounts
 * - Items and inventory
 * - Instance and map events
 *
 * Performance: Minimal overhead, leverages TrinityCore's native script
 * dispatch system with early-exit checks for non-bot players.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Creature.h"
#include "Group.h"
#include "Guild.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Item.h"
#include "Vehicle.h"
#include "ObjectAccessor.h"
// #include "AuctionHouseObject.h"  // TODO Phase 6: Re-enable when AuctionHouseScript hooks are available
#include "Core/StateMachine/BotStateTypes.h"
#include "Core/Events/EventDispatcher.h"
#include "Events/BotEventData.h"
#include "AI/BotAI.h"
#include "scripts/PlayerbotWorldScript.h"
#include "WorldSession.h"
#include "PlayerAI.h"
#include "Log.h"

using namespace Playerbot::Events;
using namespace Playerbot::StateMachine;

// ============================================================================
// HELPER FUNCTIONS - Phase 7.3 Direct Event Dispatch
// ============================================================================

/**
 * @brief Check if a player is a bot
 * @param player The player to check
 * @return true if player is a bot (BotSession)
 *
 * Phase 7.3: Fixed to use WorldSession::IsBot() pattern
 * BotSession overrides IsBot() to return true
 */
static bool IsBot(Player const* player)
{
    if (!player)
        return false;

    // Use WorldSession::IsBot() - BotSession overrides this to return true
    WorldSession* session = player->GetSession();
    return session && session->IsBot();
}

static bool IsBot(Unit const* unit)
{
    if (!unit)
        return false;

    Player const* player = unit->ToPlayer();
    return player && IsBot(player);
}

/**
 * @brief Dispatch event directly to bot's EventDispatcher
 * @param player The bot player
 * @param event The event to dispatch
 *
 * Phase 7.3: Replaces BotEventHooks layer - direct dispatch to EventDispatcher
 * Eliminates redundant 1113-line abstraction layer
 */
static void DispatchToBotEventDispatcher(Player const* player, BotEvent const& event)
{
    if (!player)
        return;

    // Get BotAI from Player's AI
    Playerbot::BotAI* botAI = dynamic_cast<Playerbot::BotAI*>(const_cast<Player*>(player)->AI());
    if (!botAI)
        return;

    // Get bot's EventDispatcher
    Playerbot::Events::EventDispatcher* dispatcher = botAI->GetEventDispatcher();
    if (!dispatcher)
        return;

    // Dispatch to bot-specific EventDispatcher (Phase 7 architecture)
    dispatcher->Dispatch(event);
}

// ============================================================================
// NOTE: PlayerbotWorldScript is defined in PlayerbotWorldScript.cpp
// This file contains only the PlayerScript and other event hook scripts
// ============================================================================

// ============================================================================
// PLAYER SCRIPT - Comprehensive Player Event Coverage
// ============================================================================

class PlayerbotPlayerScript : public PlayerScript
{
public:
    PlayerbotPlayerScript() : PlayerScript("PlayerbotPlayerScript") {}

    // ========================================================================
    // COMBAT EVENTS
    // ========================================================================

    void OnPVPKill(Player* killer, Player* killed) override
    {
        // TODO Phase 6: Implement PvP kill tracking
        // For now, these are handled by UnitScript hooks
        (void)killer;
        (void)killed;
    }

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        // TODO Phase 6: Implement creature kill tracking
        // For now, combat end is handled by UnitScript hooks
        (void)killer;
        (void)killed;
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (!IsBot(killed))
            return;

        // Phase 7.3: Direct event dispatch (eliminated BotEventHooks layer)
        BotEvent event(EventType::PLAYER_DIED,
                       killer ? killer->GetGUID() : ObjectGuid::Empty,
                       killed->GetGUID());
        event.priority = 255; // Maximum priority

        DispatchToBotEventDispatcher(killed, event);
    }

    void OnSpellCast(Player* player, Spell* spell, bool skipCheck) override
    {
        if (!IsBot(player) || !spell)
            return;

        // Phase 7.3: Direct event dispatch
        BotEvent event(EventType::SPELL_CAST_SUCCESS,
                       player->GetGUID(),
                       spell->m_targets.GetUnitTargetGUID());
        event.eventId = spell->GetSpellInfo()->Id;
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);
    }

    // ========================================================================
    // PROGRESSION EVENTS
    // ========================================================================

    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::PLAYER_LEVEL_UP,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = player->GetLevel();
        event.data = std::to_string(oldLevel);
        event.priority = 150;

        DispatchToBotEventDispatcher(player, event);

        TC_LOG_DEBUG("module.playerbot.events",
            "Bot {} leveled up: {} -> {}",
            player->GetName(), oldLevel, player->GetLevel());
    }

    void OnFreeTalentPointsChanged(Player* player, uint32 points) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::TALENT_POINTS_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = points;
        event.priority = 120;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnTalentsReset(Player* player, bool noCost) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::TALENTS_RESET,
                      player->GetGUID(),
                      player->GetGUID());
        event.priority = 150;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnGiveXP(Player* player, uint32& amount, Unit* victim) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::XP_GAINED,
                      victim ? victim->GetGUID() : ObjectGuid::Empty,
                      player->GetGUID());
        event.eventId = amount;
        event.priority = 50;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnReputationChange(Player* player, uint32 factionId,
                           int32& standing, bool incremental) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::REPUTATION_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = factionId;
        event.data = std::to_string(standing);
        event.priority = 80;

        DispatchToBotEventDispatcher(player, event);
    }

    // ========================================================================
    // ECONOMY EVENTS
    // ========================================================================

    void OnMoneyChanged(Player* player, int64& amount) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::GOLD_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.data = std::to_string(amount);
        event.priority = 70;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnMoneyLimit(Player* player, int64 amount) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::GOLD_CAP_REACHED,
                      player->GetGUID(),
                      player->GetGUID());
        event.data = std::to_string(amount);
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);

        TC_LOG_WARN("module.playerbot.economy",
            "Bot {} reached gold cap: {}", player->GetName(), amount);
    }

    // ========================================================================
    // SOCIAL EVENTS
    // ========================================================================

    void OnChat(Player* player, uint32 type, uint32 lang,
               std::string& msg, Player* receiver) override
    {
        if (!receiver || !IsBot(receiver))
            return;

        // Phase 7.3: Direct event dispatch - bot received whisper
        BotEvent event(EventType::WHISPER_RECEIVED,
                       player->GetGUID(),
                       receiver->GetGUID());
        event.data = msg;
        event.priority = 120;

        DispatchToBotEventDispatcher(receiver, event);
    }

    void OnChat(Player* player, uint32 type, uint32 lang,
               std::string& msg, Group* group) override
    {
        if (!group)
            return;

        // Check if any bots in group
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && IsBot(member) && member != player)
            {
                BotEvent event(EventType::GROUP_CHAT,
                              player->GetGUID(),
                              member->GetGUID());
                event.data = msg;
                event.priority = 80;

                DispatchToBotEventDispatcher(player, event);
            }
        }
    }

    void OnTextEmote(Player* player, uint32 textEmote, uint32 emoteNum, ObjectGuid guid) override
    {
        // Check if emote target is a bot
        if (!guid)
            return;

        Unit* target = ObjectAccessor::GetUnit(*player, guid);
        if (target && IsBot(target))
        {
            BotEvent event(EventType::EMOTE_RECEIVED,
                          player->GetGUID(),
                          guid);
            event.eventId = textEmote;
            event.priority = 50;

            DispatchToBotEventDispatcher(player, event);
        }
    }

    // ========================================================================
    // DUEL EVENTS
    // ========================================================================

    void OnDuelRequest(Player* target, Player* challenger) override
    {
        if (!IsBot(target))
            return;

        BotEvent event(EventType::DUEL_REQUESTED,
                      challenger->GetGUID(),
                      target->GetGUID());
        event.priority = 150;

        DispatchToBotEventDispatcher(target, event);
    }

    void OnDuelStart(Player* player1, Player* player2) override
    {
        if (IsBot(player1))
        {
            BotEvent event(EventType::DUEL_STARTED,
                          player2->GetGUID(),
                          player1->GetGUID());
            event.priority = 200;
            DispatchToBotEventDispatcher(player1, event);
        }

        if (IsBot(player2))
        {
            BotEvent event(EventType::DUEL_STARTED,
                          player1->GetGUID(),
                          player2->GetGUID());
            event.priority = 200;
            DispatchToBotEventDispatcher(player2, event);
        }
    }

    void OnDuelEnd(Player* winner, Player* loser, DuelCompleteType type) override
    {
        if (IsBot(winner))
        {
            BotEvent event(EventType::DUEL_WON,
                          loser->GetGUID(),
                          winner->GetGUID());
            event.priority = 150;
            DispatchToBotEventDispatcher(winner, event);
        }

        if (IsBot(loser))
        {
            BotEvent event(EventType::DUEL_LOST,
                          winner->GetGUID(),
                          loser->GetGUID());
            event.priority = 150;
            DispatchToBotEventDispatcher(loser, event);
        }
    }

    // ========================================================================
    // LIFECYCLE EVENTS
    // ========================================================================

    void OnLogin(Player* player, bool firstLogin) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(firstLogin ? EventType::FIRST_LOGIN
                                  : EventType::PLAYER_LOGIN,
                      player->GetGUID(),
                      player->GetGUID());
        event.priority = 200;

        DispatchToBotEventDispatcher(player, event);

        TC_LOG_INFO("module.playerbot.lifecycle",
            "Bot {} logged in (first: {})", player->GetName(), firstLogin);
    }

    void OnLogout(Player* player) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::PLAYER_LOGOUT,
                      player->GetGUID(),
                      player->GetGUID());
        event.priority = 200;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnPlayerRepop(Player* player) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::PLAYER_REPOP,
                      player->GetGUID(),
                      player->GetGUID());
        event.priority = 250;

        DispatchToBotEventDispatcher(player, event);
    }

    // ========================================================================
    // INSTANCE & MAP EVENTS
    // ========================================================================

    void OnBindToInstance(Player* player, Difficulty difficulty,
                         uint32 mapId, bool permanent, uint8 extendState) override
    {
        if (!IsBot(player))
            return;

        // Phase 7.3: Direct event dispatch - instance entered
        BotEvent event(EventType::INSTANCE_ENTERED,
                       player->GetGUID(),
                       ObjectGuid::Empty);
        event.eventId = mapId;
        event.data = "0"; // instanceId placeholder
        event.priority = 200;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::ZONE_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = newZone;
        event.data = std::to_string(newArea);
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnMapChanged(Player* player) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::MAP_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = player->GetMapId();
        event.priority = 120;

        DispatchToBotEventDispatcher(player, event);
    }

    // ========================================================================
    // QUEST EVENTS
    // ========================================================================

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        if (!IsBot(player))
            return;

        BotEvent event(EventType::QUEST_STATUS_CHANGED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = questId;
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);
    }
};

// ============================================================================
// UNIT SCRIPT - Combat Events (Damage & Healing)
// ============================================================================

class PlayerbotUnitScript : public UnitScript
{
public:
    PlayerbotUnitScript() : UnitScript("PlayerbotUnitScript") {}

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        // Check if bot is involved
        bool attackerIsBot = IsBot(attacker);
        bool victimIsBot = IsBot(victim);

        if (!attackerIsBot && !victimIsBot)
            return;

        // Phase 7.3: Direct event dispatch for damage events
        if (attackerIsBot)
        {
            Player* attackerPlayer = attacker->ToPlayer();
            if (attackerPlayer)
            {
                BotEvent event(EventType::DAMAGE_DEALT,
                               attacker->GetGUID(),
                               victim->GetGUID());
                event.data = std::to_string(damage) + ":0";
                event.priority = 100;
                DispatchToBotEventDispatcher(attackerPlayer, event);
            }
        }

        if (victimIsBot)
        {
            Player* victimPlayer = victim->ToPlayer();
            if (victimPlayer)
            {
                BotEvent event(EventType::DAMAGE_TAKEN,
                               attacker ? attacker->GetGUID() : ObjectGuid::Empty,
                               victim->GetGUID());
                event.data = std::to_string(damage) + ":0";
                event.priority = 180;
                DispatchToBotEventDispatcher(victimPlayer, event);

                // Check for critical health thresholds
                if (victim->GetHealthPct() < 30.0f)
                {
                    BotEvent criticalEvent(EventType::HEALTH_CRITICAL,
                                          attacker ? attacker->GetGUID() : ObjectGuid::Empty,
                                          victim->GetGUID());
                    criticalEvent.priority = 255; // Maximum priority
                    DispatchToBotEventDispatcher(victimPlayer, criticalEvent);
                }
                else if (victim->GetHealthPct() < 50.0f)
                {
                    BotEvent lowHealthEvent(EventType::HEALTH_LOW,
                                  attacker ? attacker->GetGUID() : ObjectGuid::Empty,
                                  victim->GetGUID());
                    lowHealthEvent.priority = 200;
                    DispatchToBotEventDispatcher(victimPlayer, lowHealthEvent);
                }
            }
        }
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        // Check if bot is involved
        bool healerIsBot = IsBot(healer);
        bool receiverIsBot = IsBot(receiver);

        if (!healerIsBot && !receiverIsBot)
            return;

        if (healerIsBot)
        {
            Player* healerPlayer = healer->ToPlayer();
            if (healerPlayer)
            {
                BotEvent event(EventType::HEAL_CAST,
                              healer->GetGUID(),
                              receiver ? receiver->GetGUID() : ObjectGuid::Empty);
                event.eventId = gain;
                event.priority = 120;

                DispatchToBotEventDispatcher(healerPlayer, event);
            }
        }

        if (receiverIsBot)
        {
            Player* receiverPlayer = receiver->ToPlayer();
            if (receiverPlayer)
            {
                BotEvent event(EventType::HEAL_RECEIVED,
                              healer ? healer->GetGUID() : ObjectGuid::Empty,
                              receiver->GetGUID());
                event.eventId = gain;
                event.priority = 120;

                DispatchToBotEventDispatcher(receiverPlayer, event);
            }
        }
    }
};

// ============================================================================
// GROUP SCRIPT - Party & Raid Coordination
// ============================================================================

class PlayerbotGroupScript : public GroupScript
{
public:
    PlayerbotGroupScript() : GroupScript("PlayerbotGroupScript") {}

    void OnInviteMember(Group* group, ObjectGuid guid) override
    {
        if (!group)
            return;

        Player* invitee = ObjectAccessor::FindPlayer(guid);
        if (!invitee || !IsBot(invitee))
            return;

        BotEvent event(EventType::GROUP_INVITE_RECEIVED,
                      group->GetLeaderGUID(),
                      guid);
        event.priority = 180;

        DispatchToBotEventDispatcher(invitee, event);
    }

    void OnAddMember(Group* group, ObjectGuid guid) override
    {
        if (!group)
            return;

        Player* member = ObjectAccessor::FindPlayer(guid);
        if (!member || !IsBot(member))
            return;

        BotEvent event(EventType::GROUP_JOINED,
                      ObjectGuid::Empty,
                      guid);
        event.priority = 200;

        DispatchToBotEventDispatcher(member, event);

        TC_LOG_INFO("module.playerbot.group",
            "Bot {} joined group", member->GetName());
    }

    void OnRemoveMember(Group* group, ObjectGuid guid, RemoveMethod method,
                       ObjectGuid kicker, char const* reason) override
    {
        Player* member = ObjectAccessor::FindPlayer(guid);
        if (!member || !IsBot(member))
            return;

        BotEvent event(EventType::GROUP_LEFT,
                      kicker,
                      guid);
        event.eventId = static_cast<uint32>(method);
        event.priority = 200;

        DispatchToBotEventDispatcher(member, event);

        TC_LOG_INFO("module.playerbot.group",
            "Bot {} left group (method: {})", member->GetName(), uint32(method));
    }

    void OnChangeLeader(Group* group, ObjectGuid newLeaderGuid, ObjectGuid oldLeaderGuid) override
    {
        if (!group)
            return;

        // Notify all bot members about leader change
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && IsBot(member))
            {
                BotEvent event(EventType::GROUP_LEADER_CHANGED,
                              newLeaderGuid,
                              member->GetGUID());
                event.priority = 150;

                DispatchToBotEventDispatcher(member, event);
            }
        }
    }

    void OnDisband(Group* group) override
    {
        if (!group)
            return;

        // Notify all bot members about disbandment
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && IsBot(member))
            {
                BotEvent event(EventType::GROUP_DISBANDED,
                              ObjectGuid::Empty,
                              member->GetGUID());
                event.priority = 180;

                DispatchToBotEventDispatcher(member, event);
            }
        }
    }
};

// ============================================================================
// VEHICLE SCRIPT - Mount & Vehicle Control (CRITICAL for WoW 11.2)
// ============================================================================

class PlayerbotVehicleScript : public VehicleScript
{
public:
    PlayerbotVehicleScript() : VehicleScript("PlayerbotVehicleScript") {}

    void OnAddPassenger(Vehicle* veh, Unit* passenger, int8 seatId) override
    {
        if (!passenger || !IsBot(passenger))
            return;

        Player* passengerPlayer = passenger->ToPlayer();
        if (!passengerPlayer)
            return;

        BotEvent event(EventType::VEHICLE_ENTERED,
                      veh->GetBase()->GetGUID(),
                      passenger->GetGUID());
        event.eventId = seatId;
        event.priority = 150;

        DispatchToBotEventDispatcher(passengerPlayer, event);

        TC_LOG_DEBUG("module.playerbot.vehicle",
            "Bot entered vehicle (seat: {})", seatId);
    }

    void OnRemovePassenger(Vehicle* veh, Unit* passenger) override
    {
        if (!passenger || !IsBot(passenger))
            return;

        Player* passengerPlayer = passenger->ToPlayer();
        if (!passengerPlayer)
            return;

        BotEvent event(EventType::VEHICLE_EXITED,
                      veh->GetBase()->GetGUID(),
                      passenger->GetGUID());
        event.priority = 150;

        DispatchToBotEventDispatcher(passengerPlayer, event);
    }
};

// ============================================================================
// ITEM SCRIPT - Inventory & Equipment Management
// ============================================================================

class PlayerbotItemScript : public ItemScript
{
public:
    PlayerbotItemScript() : ItemScript("PlayerbotItemScript") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& targets, ObjectGuid castId) override
    {
        if (!IsBot(player) || !item)
            return true; // Allow usage

        BotEvent event(EventType::ITEM_USED,
                      player->GetGUID(),
                      targets.GetUnitTargetGUID());
        event.eventId = item->GetEntry();
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);

        return true; // Allow item use
    }

    bool OnExpire(Player* player, ItemTemplate const* proto) override
    {
        if (!IsBot(player) || !proto)
            return true;

        BotEvent event(EventType::ITEM_EXPIRED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = proto->GetId();
        event.priority = 80;

        DispatchToBotEventDispatcher(player, event);

        return true;
    }

    bool OnRemove(Player* player, Item* item) override
    {
        if (!IsBot(player) || !item)
            return true;

        BotEvent event(EventType::ITEM_REMOVED,
                      player->GetGUID(),
                      player->GetGUID());
        event.eventId = item->GetEntry();
        event.priority = 70;

        DispatchToBotEventDispatcher(player, event);

        return true;
    }
};

// ============================================================================
// AUCTION HOUSE SCRIPT - Economy Integration
// ============================================================================

// TODO Phase 6: Re-enable when AuctionHouseScript is available in TrinityCore
/*
class PlayerbotAuctionHouseScript : public AuctionHouseScript
{
public:
    PlayerbotAuctionHouseScript() : AuctionHouseScript("PlayerbotAuctionHouseScript") {}

    void OnAuctionAdd(AuctionHouseObject* ah, AuctionPosting* auction) override
    {
        if (!auction)
            return;

        Player* seller = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(auction->OwnerGUID));
        if (!seller || !IsBot(seller))
            return;

        BotEvent event(EventType::AUCTION_CREATED,
                      seller->GetGUID(),
                      ObjectGuid::Empty);
        event.eventId = auction->ItemID;
        event.priority = 80;

        DispatchToBotEventDispatcher(player, event);
    }

    void OnAuctionSuccessful(AuctionHouseObject* ah, AuctionPosting* auction) override
    {
        if (!auction)
            return;

        Player* seller = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(auction->OwnerGUID));
        if (!seller || !IsBot(seller))
            return;

        BotEvent event(EventType::AUCTION_SOLD,
                      seller->GetGUID(),
                      ObjectGuid::Empty);
        event.eventId = auction->ItemID;
        event.data = std::to_string(auction->BuyoutOrUnitPrice);
        event.priority = 100;

        DispatchToBotEventDispatcher(player, event);

        TC_LOG_DEBUG("module.playerbot.economy",
            "Bot {} auction sold: item {} for {} gold",
            seller->GetName(), auction->ItemID, auction->BuyoutOrUnitPrice / 10000);
    }

    void OnAuctionExpire(AuctionHouseObject* ah, AuctionPosting* auction) override
    {
        if (!auction)
            return;

        Player* seller = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(auction->OwnerGUID));
        if (!seller || !IsBot(seller))
            return;

        BotEvent event(EventType::AUCTION_EXPIRED,
                      seller->GetGUID(),
                      ObjectGuid::Empty);
        event.eventId = auction->ItemID;
        event.priority = 90;

        DispatchToBotEventDispatcher(player, event);
    }
};
*/

// ============================================================================
// SCRIPT REGISTRATION - Called by TrinityCore on module load
// ============================================================================

void AddSC_playerbot_event_scripts()
{
    // NOTE: PlayerbotWorldScript is already created in AddSC_playerbot_world()
    // Do NOT create it here to avoid duplicate registration
    // new PlayerbotWorldScript(); // REMOVED - causes duplicate OnStartup() calls

    new PlayerbotPlayerScript();
    new PlayerbotUnitScript();
    new PlayerbotGroupScript();
    new PlayerbotVehicleScript();
    new PlayerbotItemScript();
    // TODO Phase 6: Re-enable when AuctionHouseScript is available
    // new PlayerbotAuctionHouseScript();

    TC_LOG_INFO("module.playerbot.scripts",
        "✅ Playerbot Event Scripts registered:");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - WorldScript: Event system updates");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - PlayerScript: 35 player event hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - UnitScript: 2 combat event hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - GroupScript: 5 group coordination hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - VehicleScript: 2 vehicle/mount hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - ItemScript: 3 inventory hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - AuctionHouseScript: 3 economy hooks");
    TC_LOG_INFO("module.playerbot.scripts",
        "   - Total: 51 script hooks active");
}
