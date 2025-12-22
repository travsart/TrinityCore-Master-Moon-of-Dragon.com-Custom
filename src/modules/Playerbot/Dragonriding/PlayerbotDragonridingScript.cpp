/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/*
 * Dragonriding SpellScripts for Playerbot Module
 *
 * Implements the Soar ability (369536) and dragonriding boost abilities.
 * Works for ALL players, not just bots.
 *
 * Features:
 * - Soar activation with FlightCapability physics
 * - Vigor resource system with progression-based max stacks
 * - Surge Forward / Skyward Ascent boost abilities
 * - Talent-locked abilities (Whirling Surge, Aerial Halt)
 * - Speed-based vigor regeneration (Thrill of the Skies)
 * - Ground Skimming regeneration
 */

#include "DragonridingDefines.h"
#include "DragonridingMgr.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "WorldSession.h"

using namespace Playerbot::Dragonriding;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

namespace
{

// Get FlightCapabilityEntry for dragonriding physics
const FlightCapabilityEntry* GetDragonridingFlightCapability()
{
    // Query the FlightCapability DB2 store for a valid dragonriding entry
    // FlightCapability ID 1 is typically the standard dragonriding physics
    return sFlightCapabilityStore.LookupEntry(FLIGHT_CAPABILITY_SOAR);
}

// Check if a player can use Soar (Dracthyr Evoker only)
bool CanPlayerUseSoar(Player* player)
{
    if (!player)
        return false;

    return CanUseSoar(player->GetRace(), player->GetClass());
}

// Get account ID from player
uint32 GetAccountId(Player* player)
{
    if (!player || !player->GetSession())
        return 0;
    return player->GetSession()->GetAccountId();
}

} // anonymous namespace

// ============================================================================
// SPELL: SOAR (369536) - Dracthyr Evoker Racial
// Activates dragonriding physics mode
// ============================================================================

class spell_playerbot_soar : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        // Validate that required spells exist
        return ValidateSpellInfo({
            SPELL_VIGOR_BUFF,
            SPELL_SURGE_FORWARD,
            SPELL_SKYWARD_ASCENT
        });
    }

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        // Only Dracthyr Evokers can use Soar
        if (!CanPlayerUseSoar(caster))
            return SPELL_FAILED_INCORRECT_AREA; // Fallback error

        // Cannot use while already in dragonriding mode
        if (caster->GetFlightCapabilityID() != 0)
            return SPELL_FAILED_NOT_HERE;

        // Cannot use indoors
        if (!caster->IsOutdoors())
            return SPELL_FAILED_ONLY_OUTDOORS;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        uint32 accountId = GetAccountId(caster);
        if (accountId == 0)
            return;

        // Check if DragonridingMgr is initialized
        if (!sDragonridingMgr->IsInitialized())
        {
            TC_LOG_WARN("playerbot.dragonriding", "DragonridingMgr not initialized, Soar cast failed for player {}",
                caster->GetName());
            return;
        }

        // Activate dragonriding physics
        const FlightCapabilityEntry* flightCap = GetDragonridingFlightCapability();
        if (flightCap)
        {
            caster->SetFlightCapabilityID(flightCap->ID, true);
            TC_LOG_DEBUG("playerbot.dragonriding", "Soar activated for player {} (account {}), FlightCapability {}",
                caster->GetName(), accountId, flightCap->ID);
        }
        else
        {
            // Fallback to ID 1 if lookup fails
            caster->SetFlightCapabilityID(FLIGHT_CAPABILITY_SOAR, true);
            TC_LOG_WARN("playerbot.dragonriding", "FlightCapability lookup failed, using default ID {} for player {}",
                FLIGHT_CAPABILITY_SOAR, caster->GetName());
        }

        // Grant vigor based on learned talents (progression-based)
        uint32 maxVigor = sDragonridingMgr->GetMaxVigor(accountId);

        // Apply vigor buff with calculated max stacks
        caster->CastSpell(caster, SPELL_VIGOR_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_FULL_MASK,
            .OriginalCaster = caster->GetGUID()
        });

        // Set vigor to max stacks
        if (Aura* vigorAura = caster->GetAura(SPELL_VIGOR_BUFF))
        {
            vigorAura->SetStackAmount(maxVigor);
            TC_LOG_DEBUG("playerbot.dragonriding", "Vigor granted to player {}: {} stacks (max based on talents)",
                caster->GetName(), maxVigor);
        }

        // Activate action bar override to show dragonriding abilities
        // This uses the OverrideSpellData system to swap the action bar
        caster->SetOverrideSpellsId(OVERRIDE_SPELL_DATA_DRAGONRIDING);

        // Add temporary spells so they can be cast
        // Base abilities (always available during dragonriding)
        caster->AddTemporarySpell(SPELL_SURGE_FORWARD);
        caster->AddTemporarySpell(SPELL_SKYWARD_ASCENT);

        // Talent-locked abilities (only add if talent learned)
        if (sDragonridingMgr->HasWhirlingSurge(accountId))
            caster->AddTemporarySpell(SPELL_WHIRLING_SURGE);

        if (sDragonridingMgr->HasAerialHalt(accountId))
            caster->AddTemporarySpell(SPELL_AERIAL_HALT);

        TC_LOG_INFO("playerbot.dragonriding", "Player {} activated Soar with {} vigor (account {}), action bar swapped",
            caster->GetName(), maxVigor, accountId);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_soar::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_soar::HandleOnCast);
    }
};

// ============================================================================
// AURA: SOAR (369536) - Handles cleanup when Soar ends
// ============================================================================

class spell_playerbot_soar_aura : public AuraScript
{
    void HandleAfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* target = GetTarget()->ToPlayer();
        if (!target)
            return;

        // Disable dragonriding physics
        target->SetFlightCapabilityID(FLIGHT_CAPABILITY_NORMAL, true);

        // Remove vigor buff
        target->RemoveAura(SPELL_VIGOR_BUFF);

        // Remove any active boost buffs
        target->RemoveAura(SPELL_SURGE_FORWARD_BUFF);
        target->RemoveAura(SPELL_SKYWARD_ASCENT_BUFF);
        target->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
        target->RemoveAura(SPELL_GROUND_SKIMMING);

        // Remove action bar override and restore normal action bar
        target->SetOverrideSpellsId(0);

        // Remove temporary dragonriding spells
        target->RemoveTemporarySpell(SPELL_SURGE_FORWARD);
        target->RemoveTemporarySpell(SPELL_SKYWARD_ASCENT);
        target->RemoveTemporarySpell(SPELL_WHIRLING_SURGE);
        target->RemoveTemporarySpell(SPELL_AERIAL_HALT);

        TC_LOG_DEBUG("playerbot.dragonriding", "Soar ended for player {}, dragonriding disabled, action bar restored",
            target->GetName());
    }

    void Register() override
    {
        // Hook into aura removal to clean up
        AfterEffectRemove += AuraEffectRemoveFn(spell_playerbot_soar_aura::HandleAfterRemove, EFFECT_0, SPELL_AURA_FLY, AURA_EFFECT_HANDLE_REAL);
    }
};

// ============================================================================
// AURA: VIGOR BUFF (900001) - Tracks vigor stacks with dynamic maximum
// ============================================================================

class spell_playerbot_vigor_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_THRILL_OF_THE_SKIES,
            SPELL_GROUND_SKIMMING
        });
    }

    void HandleCalcMaxStackAmount(int32& maxStackAmount)
    {
        Unit* owner = GetUnitOwner();
        if (!owner)
            return;

        Player* player = owner->ToPlayer();
        if (!player)
            return;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return;

        // Get max vigor from DragonridingMgr based on learned talents
        maxStackAmount = sDragonridingMgr->GetMaxVigor(accountId);
    }

    void OnPeriodicTick(AuraEffect const* /*aurEff*/)
    {
        Unit* owner = GetUnitOwner();
        if (!owner)
            return;

        Player* player = owner->ToPlayer();
        if (!player)
            return;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return;

        // Check if we should regenerate vigor
        Aura* vigorAura = GetAura();
        if (!vigorAura)
            return;

        uint32 currentStacks = vigorAura->GetStackAmount();
        uint32 maxStacks = sDragonridingMgr->GetMaxVigor(accountId);

        // Don't regenerate if at max
        if (currentStacks >= maxStacks)
            return;

        // Determine regeneration rate based on conditions
        uint32 regenMs = 0;
        bool isFlying = player->IsFlying();
        bool isAdvFlyingActive = player->GetFlightCapabilityID() != 0;

        if (!isAdvFlyingActive)
        {
            // Not in dragonriding mode - no regen
            return;
        }

        if (!isFlying)
        {
            // Grounded regeneration (fastest)
            regenMs = sDragonridingMgr->GetGroundedRegenMs(accountId);

            // Apply Thrill of the Skies buff visual if not present
            if (!player->HasAura(SPELL_THRILL_OF_THE_SKIES))
            {
                player->RemoveAura(SPELL_GROUND_SKIMMING);
            }
        }
        else
        {
            // Flying - check speed and height conditions
            float speed = player->GetSpeed(MOVE_FLIGHT);
            float maxSpeed = player->GetSpeedRate(MOVE_FLIGHT);
            float speedPercent = (maxSpeed > 0) ? (speed / maxSpeed) : 0.0f;

            // Check for Thrill of the Skies (high speed)
            if (speedPercent >= sDragonridingMgr->GetThrillSpeedThreshold())
            {
                regenMs = sDragonridingMgr->GetFlyingRegenMs(accountId);

                if (!player->HasAura(SPELL_THRILL_OF_THE_SKIES))
                {
                    player->CastSpell(player, SPELL_THRILL_OF_THE_SKIES, true);
                    player->RemoveAura(SPELL_GROUND_SKIMMING);
                }
            }
            // Check for Ground Skimming (low altitude)
            else if (sDragonridingMgr->HasGroundSkimming(accountId))
            {
                float groundZ = player->GetMap()->GetHeight(player->GetPhaseShift(),
                    player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
                float heightAboveGround = player->GetPositionZ() - groundZ;

                if (heightAboveGround <= sDragonridingMgr->GetGroundSkimHeight())
                {
                    regenMs = BASE_REGEN_GROUND_SKIM_MS;

                    if (!player->HasAura(SPELL_GROUND_SKIMMING))
                    {
                        player->CastSpell(player, SPELL_GROUND_SKIMMING, true);
                        player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                    }
                }
                else
                {
                    // Remove both buffs if not meeting conditions
                    player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                    player->RemoveAura(SPELL_GROUND_SKIMMING);
                }
            }
            else
            {
                // Remove regen buffs if not meeting conditions
                player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                player->RemoveAura(SPELL_GROUND_SKIMMING);
            }
        }

        // Regeneration logic handled by periodic effect with correct timing
        // The actual stack increment happens based on accumulated time
    }

    void Register() override
    {
        // Hook for calculating max stack amount
        // Note: This uses the periodic tick to also handle regen logic
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_playerbot_vigor_aura::OnPeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// ============================================================================
// SPELL: SURGE FORWARD (900002) - Speed boost, costs 1 vigor
// ============================================================================

class spell_playerbot_surge_forward : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR_BUFF, SPELL_SURGE_FORWARD_BUFF });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        // Must have vigor
        Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF);
        if (!vigor || vigor->GetStackAmount() < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor stack
        if (Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF))
        {
            vigor->ModStackAmount(-1);
        }

        // Apply speed boost buff
        caster->CastSpell(caster, SPELL_SURGE_FORWARD_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_FULL_MASK,
            .OriginalCaster = caster->GetGUID()
        });

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Surge Forward, vigor consumed",
                player->GetName());
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_surge_forward::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_surge_forward::HandleOnCast);
    }
};

// ============================================================================
// SPELL: SKYWARD ASCENT (900003) - Vertical thrust, costs 1 vigor
// ============================================================================

class spell_playerbot_skyward_ascent : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR_BUFF, SPELL_SKYWARD_ASCENT_BUFF });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        // Must have vigor
        Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF);
        if (!vigor || vigor->GetStackAmount() < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor stack
        if (Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF))
        {
            vigor->ModStackAmount(-1);
        }

        // Apply vertical thrust buff
        caster->CastSpell(caster, SPELL_SKYWARD_ASCENT_BUFF, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_FULL_MASK,
            .OriginalCaster = caster->GetGUID()
        });

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Skyward Ascent, vigor consumed",
                player->GetName());
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_skyward_ascent::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_skyward_ascent::HandleOnCast);
    }
};

// ============================================================================
// SPELL: WHIRLING SURGE (900004) - Barrel roll, requires Airborne Tumbling talent
// ============================================================================

class spell_playerbot_whirling_surge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR_BUFF });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        Player* player = caster->ToPlayer();
        if (!player)
            return SPELL_FAILED_BAD_TARGETS;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return SPELL_FAILED_ERROR;

        // Must have Airborne Tumbling talent
        if (!sDragonridingMgr->HasWhirlingSurge(accountId))
            return SPELL_FAILED_NOT_KNOWN;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        // Must have vigor
        Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF);
        if (!vigor || vigor->GetStackAmount() < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor stack
        if (Aura* vigor = caster->GetAura(SPELL_VIGOR_BUFF))
        {
            vigor->ModStackAmount(-1);
        }

        // Whirling Surge provides a speed boost and barrel roll animation
        // The actual movement effect is handled by the spell effect in DB2

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Whirling Surge (talent-unlocked ability)",
                player->GetName());
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_whirling_surge::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_whirling_surge::HandleOnCast);
    }
};

// ============================================================================
// SPELL: AERIAL HALT (900005) - Hover in place, requires At Home Aloft talent
// ============================================================================

class spell_playerbot_aerial_halt : public SpellScript
{
    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        Player* player = caster->ToPlayer();
        if (!player)
            return SPELL_FAILED_BAD_TARGETS;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return SPELL_FAILED_ERROR;

        // Must have At Home Aloft talent
        if (!sDragonridingMgr->HasAerialHalt(accountId))
            return SPELL_FAILED_NOT_KNOWN;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        // Aerial Halt stops momentum and allows hovering
        // This is handled by the spell effect modifying movement

        TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Aerial Halt (talent-unlocked ability)",
            player->GetName());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_aerial_halt::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_aerial_halt::HandleOnCast);
    }
};

// ============================================================================
// VIGOR REGENERATION HANDLER
// Handles periodic vigor regeneration based on flight conditions
// ============================================================================

class spell_playerbot_vigor_regen : public AuraScript
{
private:
    uint32 _accumulatedTimeMs = 0;

public:
    void HandlePeriodicTick(AuraEffect const* /*aurEff*/)
    {
        Unit* owner = GetUnitOwner();
        if (!owner)
            return;

        Player* player = owner->ToPlayer();
        if (!player)
            return;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return;

        // Get vigor aura
        Aura* vigorAura = player->GetAura(SPELL_VIGOR_BUFF);
        if (!vigorAura)
            return;

        uint32 currentStacks = vigorAura->GetStackAmount();
        uint32 maxStacks = sDragonridingMgr->GetMaxVigor(accountId);

        // Don't regenerate if at max
        if (currentStacks >= maxStacks)
        {
            _accumulatedTimeMs = 0;
            return;
        }

        // Determine current regeneration rate
        uint32 regenMs = 0;
        bool isFlying = player->IsFlying();
        bool isAdvFlyingActive = player->GetFlightCapabilityID() != 0;

        if (!isAdvFlyingActive)
        {
            // Not in dragonriding mode
            _accumulatedTimeMs = 0;
            return;
        }

        if (!isFlying)
        {
            // Grounded regeneration
            regenMs = sDragonridingMgr->GetGroundedRegenMs(accountId);
        }
        else
        {
            // Check Thrill of the Skies condition (high speed)
            if (player->HasAura(SPELL_THRILL_OF_THE_SKIES))
            {
                regenMs = sDragonridingMgr->GetFlyingRegenMs(accountId);
            }
            // Check Ground Skimming condition (low altitude)
            else if (player->HasAura(SPELL_GROUND_SKIMMING) && sDragonridingMgr->HasGroundSkimming(accountId))
            {
                regenMs = BASE_REGEN_GROUND_SKIM_MS;
            }
            else
            {
                // No regeneration while flying without conditions met
                _accumulatedTimeMs = 0;
                return;
            }
        }

        // Accumulate time (tick happens every VIGOR_UPDATE_INTERVAL_MS)
        _accumulatedTimeMs += VIGOR_UPDATE_INTERVAL_MS;

        // Check if we've accumulated enough time for a vigor point
        if (_accumulatedTimeMs >= regenMs)
        {
            vigorAura->ModStackAmount(1);
            _accumulatedTimeMs -= regenMs;

            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} regenerated 1 vigor (now: {}/{}, rate: {}ms)",
                player->GetName(), vigorAura->GetStackAmount(), maxStacks, regenMs);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_playerbot_vigor_regen::HandlePeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// ============================================================================
// SCRIPT REGISTRATION
// ============================================================================

void AddSC_playerbot_dragonriding()
{
    // Soar ability (369536)
    RegisterSpellScript(spell_playerbot_soar);
    RegisterSpellScript(spell_playerbot_soar_aura);

    // Vigor system (900001)
    RegisterSpellScript(spell_playerbot_vigor_aura);
    RegisterSpellScript(spell_playerbot_vigor_regen);

    // Boost abilities (900002-900005)
    RegisterSpellScript(spell_playerbot_surge_forward);
    RegisterSpellScript(spell_playerbot_skyward_ascent);
    RegisterSpellScript(spell_playerbot_whirling_surge);
    RegisterSpellScript(spell_playerbot_aerial_halt);

    TC_LOG_INFO("playerbot.dragonriding", ">> Registered Playerbot Dragonriding SpellScripts");
}
