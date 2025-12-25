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
 * ENTERPRISE-GRADE RETAIL SPELL INTEGRATION
 *
 * This module uses REAL retail spell IDs (from wowhead.com WoW 11.2.7):
 *   369536 = Soar (Dracthyr racial - initiates dragonriding)
 *   372608 = Surge Forward (primary forward burst)
 *   372610 = Skyward Ascent (upward burst)
 *   361584 = Whirling Surge (spiral forward)
 *   403092 = Aerial Halt (brake/stop)
 *   383359 = Vigor (the resource/charges system)
 *   383366 = Thrill of the Skies (high-speed regen buff)
 *
 * Using retail spell IDs means the client already has:
 *   - Proper icons, names, tooltips in all languages
 *   - Spell effects, animations, sounds
 *   - Correct spell mechanics and categories
 *
 * We only need override_spell_data to map our override ID to these spells.
 */

#include "DragonridingDefines.h"
#include "DragonridingMgr.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
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
        // DEBUG: Log what spells we're checking
        TC_LOG_ERROR("playerbot.dragonriding", ">>> spell_playerbot_soar::Validate() called");
        TC_LOG_ERROR("playerbot.dragonriding", ">>>   Checking SPELL_VIGOR ({}): {}",
            SPELL_VIGOR, sSpellMgr->GetSpellInfo(SPELL_VIGOR, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
        TC_LOG_ERROR("playerbot.dragonriding", ">>>   Checking SPELL_SURGE_FORWARD ({}): {}",
            SPELL_SURGE_FORWARD, sSpellMgr->GetSpellInfo(SPELL_SURGE_FORWARD, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
        TC_LOG_ERROR("playerbot.dragonriding", ">>>   Checking SPELL_SKYWARD_ASCENT ({}): {}",
            SPELL_SKYWARD_ASCENT, sSpellMgr->GetSpellInfo(SPELL_SKYWARD_ASCENT, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
        TC_LOG_ERROR("playerbot.dragonriding", ">>>   Checking SPELL_WHIRLING_SURGE ({}): {}",
            SPELL_WHIRLING_SURGE, sSpellMgr->GetSpellInfo(SPELL_WHIRLING_SURGE, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
        TC_LOG_ERROR("playerbot.dragonriding", ">>>   Checking SPELL_AERIAL_HALT ({}): {}",
            SPELL_AERIAL_HALT, sSpellMgr->GetSpellInfo(SPELL_AERIAL_HALT, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");

        // Validate that required retail spells exist in SpellMgr
        // Using retail spell IDs - client has all data, we just verify server knows them
        bool result = ValidateSpellInfo({
            SPELL_VIGOR,              // 383359 - Vigor/Skyriding charges (retail)
            SPELL_SURGE_FORWARD,      // 372608 - Forward burst (retail)
            SPELL_SKYWARD_ASCENT,     // 372610 - Upward burst (retail)
            SPELL_WHIRLING_SURGE,     // 361584 - Spiral forward (retail)
            SPELL_AERIAL_HALT         // 403092 - Brake/stop (retail)
        });

        TC_LOG_ERROR("playerbot.dragonriding", ">>> spell_playerbot_soar::Validate() result: {}", result ? "SUCCESS" : "FAILED");
        return result;
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

        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR SPELL SCRIPT TRIGGERED for player {}", caster->GetName());

        uint32 accountId = GetAccountId(caster);
        if (accountId == 0)
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Account ID is 0, aborting Soar");
            return;
        }

        // Check if DragonridingMgr is initialized
        if (!sDragonridingMgr->IsInitialized())
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> DragonridingMgr not initialized, Soar cast failed for player {}",
                caster->GetName());
            return;
        }

        // Activate dragonriding physics
        const FlightCapabilityEntry* flightCap = GetDragonridingFlightCapability();
        if (flightCap)
        {
            caster->SetFlightCapabilityID(flightCap->ID, true);
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: FlightCapability set to {} for player {}",
                flightCap->ID, caster->GetName());
        }
        else
        {
            // Fallback to ID 1 if lookup fails
            caster->SetFlightCapabilityID(FLIGHT_CAPABILITY_SOAR, true);
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: FlightCapability lookup failed, using default ID {} for player {}",
                FLIGHT_CAPABILITY_SOAR, caster->GetName());
        }

        // Grant vigor based on learned talents (progression-based)
        uint32 maxVigor = sDragonridingMgr->GetMaxVigor(accountId);

        // DEBUG: Check if retail vigor spell (383359) exists in SpellMgr
        SpellInfo const* vigorSpellInfo = sSpellMgr->GetSpellInfo(SPELL_VIGOR, DIFFICULTY_NONE);
        if (!vigorSpellInfo)
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: CRITICAL - Retail Vigor spell {} NOT FOUND in SpellMgr! "
                "This spell should exist in client DB2 data.", SPELL_VIGOR);
        }
        else
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: Retail Vigor spell {} found in SpellMgr (good!)",
                SPELL_VIGOR);
        }

        // Set vigor using POWER_ALTERNATE_MOUNT (power type 25)
        // This is how retail dragonriding vigor works - it's a power type, not just an aura
        caster->SetMaxPower(POWER_ALTERNATE_MOUNT, maxVigor);
        caster->SetPower(POWER_ALTERNATE_MOUNT, maxVigor);
        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: Set POWER_ALTERNATE_MOUNT to {}/{} for player {}",
            maxVigor, maxVigor, caster->GetName());

        // Also apply retail vigor buff (383359) for visual tracking
        SpellCastResult castResult = caster->CastSpell(caster, SPELL_VIGOR, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_FULL_MASK,
            .OriginalCaster = caster->GetGUID()
        });

        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: CastSpell(SPELL_VIGOR={}) result: {}",
            SPELL_VIGOR, uint32(castResult));

        // Set vigor to max stacks
        if (Aura* vigorAura = caster->GetAura(SPELL_VIGOR))
        {
            vigorAura->SetStackAmount(maxVigor);
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: Vigor aura granted {} stacks for player {}",
                maxVigor, caster->GetName());
        }
        else
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: No vigor aura (may be handled by power type instead)");
        }

        // Swap action bar to dragonriding abilities using DIRECT approach
        // SetOverrideSpellsId updates the client update field directly
        // AddTemporarySpell makes the spells castable on the server
        // The override points to retail spell IDs: 372608, 372610, 361584, 403092
        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: Setting OverrideSpellsId to {} for player {}",
            OVERRIDE_SPELL_DATA_SOAR, caster->GetName());

        caster->SetOverrideSpellsId(OVERRIDE_SPELL_DATA_SOAR);

        // Manually add temporary spells so player can cast them
        // This is required even if override_spell_data exists in DB2
        caster->AddTemporarySpell(SPELL_SURGE_FORWARD);
        caster->AddTemporarySpell(SPELL_SKYWARD_ASCENT);
        caster->AddTemporarySpell(SPELL_WHIRLING_SURGE);
        caster->AddTemporarySpell(SPELL_AERIAL_HALT);

        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: Added temporary spells {}, {}, {}, {}",
            SPELL_SURGE_FORWARD, SPELL_SKYWARD_ASCENT, SPELL_WHIRLING_SURGE, SPELL_AERIAL_HALT);

        // Check if the OverrideSpellData exists in the store (for debugging)
        // This should contain retail spell IDs: 372608, 372610, 361584, 403092
        // Log the table hash for troubleshooting hotfix_data entries
        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: OverrideSpellData table hash: 0x{:X} (update SQL if mismatched!)",
            sOverrideSpellDataStore.GetTableHash());

        if (OverrideSpellDataEntry const* overrideSpells = sOverrideSpellDataStore.LookupEntry(OVERRIDE_SPELL_DATA_SOAR))
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: OverrideSpellData {} FOUND! Retail spells: {}, {}, {}, {}",
                OVERRIDE_SPELL_DATA_SOAR,
                overrideSpells->Spells[0], overrideSpells->Spells[1],
                overrideSpells->Spells[2], overrideSpells->Spells[3]);
        }
        else
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: OverrideSpellData {} NOT FOUND in DB2 store! "
                "1. Import sql/hotfixes/dragonriding_retail_spells.sql into hotfixes database "
                "2. RESTART worldserver for changes to take effect",
                OVERRIDE_SPELL_DATA_SOAR);
        }

        TC_LOG_ERROR("playerbot.dragonriding", ">>> Soar: COMPLETE - Player {} activated Soar with {} vigor, OverrideSpellsId={}",
            caster->GetName(), maxVigor, OVERRIDE_SPELL_DATA_SOAR);
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
        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL TRIGGERED");

        Player* target = GetTarget()->ToPlayer();
        if (!target)
        {
            TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: Target is not a player!");
            return;
        }

        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: Cleaning up for player {}", target->GetName());

        // Disable dragonriding physics
        target->SetFlightCapabilityID(FLIGHT_CAPABILITY_NORMAL, true);
        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: FlightCapability reset to normal");

        // Reset vigor power type
        target->SetMaxPower(POWER_ALTERNATE_MOUNT, 0);
        target->SetPower(POWER_ALTERNATE_MOUNT, 0);
        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: POWER_ALTERNATE_MOUNT reset to 0");

        // Remove retail vigor buff (383359)
        target->RemoveAura(SPELL_VIGOR);

        // Remove any active retail buff spells
        target->RemoveAura(SPELL_THRILL_OF_THE_SKIES);    // 383366 - high-speed regen
        target->RemoveAura(SPELL_GROUND_SKIMMING_BUFF);   // 900002 - custom ground skim tracking

        // Reset action bar override using DIRECT approach (matching HandleOnCast)
        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: Resetting OverrideSpellsId to 0");
        target->SetOverrideSpellsId(0);

        // Remove all temporary spells we added in HandleOnCast
        target->RemoveTemporarySpell(SPELL_SURGE_FORWARD);
        target->RemoveTemporarySpell(SPELL_SKYWARD_ASCENT);
        target->RemoveTemporarySpell(SPELL_WHIRLING_SURGE);
        target->RemoveTemporarySpell(SPELL_AERIAL_HALT);
        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL: Removed all temporary dragonriding spells");

        TC_LOG_ERROR("playerbot.dragonriding", ">>> SOAR AURA REMOVAL COMPLETE for player {}", target->GetName());
    }

    void Register() override
    {
        // Hook into aura removal to clean up - use SPELL_AURA_ANY to match whatever aura type Soar uses
        AfterEffectRemove += AuraEffectRemoveFn(spell_playerbot_soar_aura::HandleAfterRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// ============================================================================
// AURA: VIGOR (383359) - Tracks vigor stacks with dynamic maximum
// This uses the RETAIL vigor spell ID for proper client integration
// ============================================================================

class spell_playerbot_vigor_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({
            SPELL_THRILL_OF_THE_SKIES,     // 383366 - retail high-speed buff
            SPELL_GROUND_SKIMMING_BUFF     // 900002 - custom ground skim tracking
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

    // Called when the aura is applied - set initial vigor stacks
    void HandleOnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        TC_LOG_ERROR("playerbot.dragonriding", ">>> VIGOR AURA APPLIED!");

        Unit* owner = GetUnitOwner();
        if (!owner)
            return;

        Player* player = owner->ToPlayer();
        if (!player)
            return;

        uint32 accountId = GetAccountId(player);
        if (accountId == 0)
            return;

        // Set initial vigor to max
        uint32 maxVigor = sDragonridingMgr->GetMaxVigor(accountId);
        if (Aura* aura = GetAura())
        {
            aura->SetStackAmount(maxVigor);
            TC_LOG_ERROR("playerbot.dragonriding", ">>> VIGOR AURA: Set {} stacks for player {}",
                maxVigor, player->GetName());
        }
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
                player->RemoveAura(SPELL_GROUND_SKIMMING_BUFF);
            }
        }
        else
        {
            // Flying - check speed and height conditions
            float speed = player->GetSpeed(MOVE_FLIGHT);
            float maxSpeed = player->GetSpeedRate(MOVE_FLIGHT);
            float speedPercent = (maxSpeed > 0) ? (speed / maxSpeed) : 0.0f;

            // Check for Thrill of the Skies (high speed) - uses retail spell 383366
            if (speedPercent >= sDragonridingMgr->GetThrillSpeedThreshold())
            {
                regenMs = sDragonridingMgr->GetFlyingRegenMs(accountId);

                if (!player->HasAura(SPELL_THRILL_OF_THE_SKIES))
                {
                    player->CastSpell(player, SPELL_THRILL_OF_THE_SKIES, true);
                    player->RemoveAura(SPELL_GROUND_SKIMMING_BUFF);
                }
            }
            // Check for Ground Skimming (low altitude) - custom tracking spell
            else if (sDragonridingMgr->HasGroundSkimming(accountId))
            {
                float groundZ = player->GetMap()->GetHeight(player->GetPhaseShift(),
                    player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
                float heightAboveGround = player->GetPositionZ() - groundZ;

                if (heightAboveGround <= sDragonridingMgr->GetGroundSkimHeight())
                {
                    regenMs = BASE_REGEN_GROUND_SKIM_MS;

                    if (!player->HasAura(SPELL_GROUND_SKIMMING_BUFF))
                    {
                        player->CastSpell(player, SPELL_GROUND_SKIMMING_BUFF, true);
                        player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                    }
                }
                else
                {
                    // Remove both buffs if not meeting conditions
                    player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                    player->RemoveAura(SPELL_GROUND_SKIMMING_BUFF);
                }
            }
            else
            {
                // Remove regen buffs if not meeting conditions
                player->RemoveAura(SPELL_THRILL_OF_THE_SKIES);
                player->RemoveAura(SPELL_GROUND_SKIMMING_BUFF);
            }
        }

        // Regeneration logic handled by periodic effect with correct timing
        // The actual stack increment happens based on accumulated time
    }

    void Register() override
    {
        // SPELL_AURA_DUMMY (4) is the actual aura type for Vigor spell 383359
        // Use AfterEffectApply to set initial vigor stacks when aura is applied
        AfterEffectApply += AuraEffectApplyFn(spell_playerbot_vigor_aura::HandleOnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);

        // Note: SPELL_AURA_DUMMY doesn't have periodic ticks, so vigor regeneration
        // must be handled separately (e.g., via WorldScript or player update hooks)
    }
};

// ============================================================================
// SPELL: SURGE FORWARD (372608) - Speed boost, costs 1 vigor
// Uses RETAIL spell ID - client has all effects/animations
// ============================================================================

class spell_playerbot_surge_forward : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR }); // 383359 - retail vigor
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        // Must have vigor (POWER_ALTERNATE_MOUNT)
        if (caster->GetPower(POWER_ALTERNATE_MOUNT) < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor charge (POWER_ALTERNATE_MOUNT)
        int32 currentVigor = caster->GetPower(POWER_ALTERNATE_MOUNT);
        if (currentVigor > 0)
        {
            caster->SetPower(POWER_ALTERNATE_MOUNT, currentVigor - 1);
        }

        // The retail spell (372608) already has its own effects
        // We just need to handle the vigor consumption

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Surge Forward (retail {}), vigor: {} -> {}",
                player->GetName(), SPELL_SURGE_FORWARD, currentVigor, currentVigor - 1);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_surge_forward::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_surge_forward::HandleOnCast);
    }
};

// ============================================================================
// SPELL: SKYWARD ASCENT (372610) - Vertical thrust, costs 1 vigor
// Uses RETAIL spell ID - client has all effects/animations
// ============================================================================

class spell_playerbot_skyward_ascent : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR }); // 383359 - retail vigor
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_BAD_TARGETS;

        // Must be in dragonriding mode
        if (caster->GetFlightCapabilityID() == 0)
            return SPELL_FAILED_NOT_READY;

        // Must have vigor (POWER_ALTERNATE_MOUNT)
        if (caster->GetPower(POWER_ALTERNATE_MOUNT) < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor charge (POWER_ALTERNATE_MOUNT)
        int32 currentVigor = caster->GetPower(POWER_ALTERNATE_MOUNT);
        if (currentVigor > 0)
        {
            caster->SetPower(POWER_ALTERNATE_MOUNT, currentVigor - 1);
        }

        // The retail spell (372610) already has its own effects
        // We just need to handle the vigor consumption

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Skyward Ascent (retail {}), vigor: {} -> {}",
                player->GetName(), SPELL_SKYWARD_ASCENT, currentVigor, currentVigor - 1);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_skyward_ascent::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_skyward_ascent::HandleOnCast);
    }
};

// ============================================================================
// SPELL: WHIRLING SURGE (361584) - Barrel roll, requires Airborne Tumbling talent
// Uses RETAIL spell ID - client has all effects/animations
// ============================================================================

class spell_playerbot_whirling_surge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VIGOR }); // 383359 - retail vigor
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

        // Must have vigor (POWER_ALTERNATE_MOUNT)
        if (caster->GetPower(POWER_ALTERNATE_MOUNT) < 1)
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void HandleOnCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Consume 1 vigor charge (POWER_ALTERNATE_MOUNT)
        int32 currentVigor = caster->GetPower(POWER_ALTERNATE_MOUNT);
        if (currentVigor > 0)
        {
            caster->SetPower(POWER_ALTERNATE_MOUNT, currentVigor - 1);
        }

        // The retail spell (361584) already has barrel roll effects
        // We just need to handle vigor consumption and talent check

        if (Player* player = caster->ToPlayer())
        {
            TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Whirling Surge (retail {}), vigor: {} -> {}",
                player->GetName(), SPELL_WHIRLING_SURGE, currentVigor, currentVigor - 1);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_whirling_surge::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_whirling_surge::HandleOnCast);
    }
};

// ============================================================================
// SPELL: AERIAL HALT (403092) - Hover in place, requires At Home Aloft talent
// Uses RETAIL spell ID - does NOT consume vigor, just cooldown
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

        // Aerial Halt (403092) stops momentum and allows hovering
        // The retail spell already has the brake effect
        // Note: Does NOT consume vigor, only has 10s cooldown

        TC_LOG_DEBUG("playerbot.dragonriding", "Player {} used Aerial Halt (retail {})",
            player->GetName(), SPELL_AERIAL_HALT);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_playerbot_aerial_halt::CheckCast);
        OnCast += SpellCastFn(spell_playerbot_aerial_halt::HandleOnCast);
    }
};

// ============================================================================
// VIGOR REGENERATION HANDLER (for retail SPELL_VIGOR 383359)
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

        // Get vigor aura (retail spell 383359)
        Aura* vigorAura = player->GetAura(SPELL_VIGOR);
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
            else if (player->HasAura(SPELL_GROUND_SKIMMING_BUFF) && sDragonridingMgr->HasGroundSkimming(accountId))
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
    // Soar ability (369536) - Dracthyr racial
    RegisterSpellScript(spell_playerbot_soar);
    RegisterSpellScript(spell_playerbot_soar_aura);

    // Vigor system (383359) - Retail vigor spell
    RegisterSpellScript(spell_playerbot_vigor_aura);
    // NOTE: spell_playerbot_vigor_regen disabled because retail Vigor (383359) uses
    // SPELL_AURA_DUMMY which doesn't have periodic ticks. Vigor regeneration will be
    // implemented via WorldScript or player update hooks in a future update.
    // RegisterSpellScript(spell_playerbot_vigor_regen);

    // Boost abilities using RETAIL spell IDs
    // 372608 = Surge Forward, 372610 = Skyward Ascent
    // 361584 = Whirling Surge, 403092 = Aerial Halt
    RegisterSpellScript(spell_playerbot_surge_forward);
    RegisterSpellScript(spell_playerbot_skyward_ascent);
    RegisterSpellScript(spell_playerbot_whirling_surge);
    RegisterSpellScript(spell_playerbot_aerial_halt);

    TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: Registered with RETAIL spell IDs:");
    TC_LOG_ERROR("server.loading", ">>>   Soar: {}, Vigor: {}", SPELL_SOAR, SPELL_VIGOR);
    TC_LOG_ERROR("server.loading", ">>>   Surge Forward: {}, Skyward Ascent: {}", SPELL_SURGE_FORWARD, SPELL_SKYWARD_ASCENT);
    TC_LOG_ERROR("server.loading", ">>>   Whirling Surge: {}, Aerial Halt: {}", SPELL_WHIRLING_SURGE, SPELL_AERIAL_HALT);

    // DEBUG: Check if required spells exist in SpellMgr
    TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: Checking if spells exist in SpellMgr...");
    TC_LOG_ERROR("server.loading", ">>>   Soar ({}): {}",
        SPELL_SOAR, sSpellMgr->GetSpellInfo(SPELL_SOAR, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
    TC_LOG_ERROR("server.loading", ">>>   Vigor ({}): {}",
        SPELL_VIGOR, sSpellMgr->GetSpellInfo(SPELL_VIGOR, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
    TC_LOG_ERROR("server.loading", ">>>   Surge Forward ({}): {}",
        SPELL_SURGE_FORWARD, sSpellMgr->GetSpellInfo(SPELL_SURGE_FORWARD, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
    TC_LOG_ERROR("server.loading", ">>>   Skyward Ascent ({}): {}",
        SPELL_SKYWARD_ASCENT, sSpellMgr->GetSpellInfo(SPELL_SKYWARD_ASCENT, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
    TC_LOG_ERROR("server.loading", ">>>   Whirling Surge ({}): {}",
        SPELL_WHIRLING_SURGE, sSpellMgr->GetSpellInfo(SPELL_WHIRLING_SURGE, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");
    TC_LOG_ERROR("server.loading", ">>>   Aerial Halt ({}): {}",
        SPELL_AERIAL_HALT, sSpellMgr->GetSpellInfo(SPELL_AERIAL_HALT, DIFFICULTY_NONE) ? "EXISTS" : "MISSING");

    // DEBUG: Log the actual effect types of Vigor spell (383359) to find correct aura type
    if (SpellInfo const* vigorInfo = sSpellMgr->GetSpellInfo(SPELL_VIGOR, DIFFICULTY_NONE))
    {
        TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: Vigor spell {} effects:", SPELL_VIGOR);
        for (SpellEffectInfo const& effectInfo : vigorInfo->GetEffects())
        {
            TC_LOG_ERROR("server.loading", ">>>   Effect {}: Type={}, AuraType={}, Amplitude={}",
                effectInfo.EffectIndex,
                uint32(effectInfo.Effect),
                uint32(effectInfo.ApplyAuraName),
                effectInfo.ApplyAuraPeriod);
        }
    }

    // DEBUG: Log the actual effect types of Soar spell (369536)
    if (SpellInfo const* soarInfo = sSpellMgr->GetSpellInfo(SPELL_SOAR, DIFFICULTY_NONE))
    {
        TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: Soar spell {} effects:", SPELL_SOAR);
        for (SpellEffectInfo const& effectInfo : soarInfo->GetEffects())
        {
            TC_LOG_ERROR("server.loading", ">>>   Effect {}: Type={}, AuraType={}, Amplitude={}",
                effectInfo.EffectIndex,
                uint32(effectInfo.Effect),
                uint32(effectInfo.ApplyAuraName),
                effectInfo.ApplyAuraPeriod);
        }
    }

    // Check if OverrideSpellData is loaded (should contain retail spell IDs)
    if (OverrideSpellDataEntry const* overrideSpells = sOverrideSpellDataStore.LookupEntry(OVERRIDE_SPELL_DATA_SOAR))
    {
        TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: OverrideSpellData {} LOADED! Retail spells: {}, {}, {}, {}",
            OVERRIDE_SPELL_DATA_SOAR,
            overrideSpells->Spells[0], overrideSpells->Spells[1],
            overrideSpells->Spells[2], overrideSpells->Spells[3]);
    }
    else
    {
        TC_LOG_ERROR("server.loading", ">>> PLAYERBOT DRAGONRIDING: OverrideSpellData {} NOT IN DB2 STORE! "
            "Import sql/hotfixes/dragonriding_retail_spells.sql into hotfixes database and restart!",
            OVERRIDE_SPELL_DATA_SOAR);
    }
}
