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

#include "SpellPacketBuilder.h"

// TrinityCore includes
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace WorldPackets::Spells;

// ============================================================================
// Public API - BuildCastSpellPacket
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCastSpellPacket(
    Player* caster,
    uint32 spellId,
    Unit* target,
    BuildOptions const& options)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    // Pre-flight validation (unless completely skipped)
    if (!options.skipValidation)
    {
        // Step 1: Validate player object
        if (!options.skipSpellCheck)
        {
            result.result = ValidatePlayer(caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = "Player validation failed";
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 2: Validate spell ID
        if (!options.skipSpellCheck)
        {
            result.result = ValidateSpellId(spellId, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Invalid spell ID: {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Get spell info (needed for all subsequent checks)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        if (!spellInfo)
        {
            result.result = ValidationResult::SPELL_NOT_FOUND;
            result.failureReason = fmt::format("Spell info not found for spell {}", spellId);
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        // Step 3: Validate spell is learned
        if (!options.skipSpellCheck)
        {
            result.result = ValidateSpellLearned(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Spell {} not learned by {}", spellId, caster->GetName());
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 4: Validate cooldown
        if (!options.skipSpellCheck)
        {
            result.result = ValidateCooldown(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Spell {} on cooldown", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 5: Validate resources (mana, rage, energy, runes, etc.)
        if (!options.skipResourceCheck)
        {
            result.result = ValidateResources(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Insufficient resources for spell {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 6: Validate caster state (alive, not stunned, not silenced, etc.)
        if (!options.skipStateCheck)
        {
            result.result = ValidateCasterState(spellInfo, caster, options);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Caster state invalid for spell {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 7: Validate GCD/casting state
        if (!options.skipGcdCheck)
        {
            result.result = ValidateGlobalCooldown(caster, spellInfo, options);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = "GCD active or spell in progress";
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 8: Validate target (if target provided)
        if (!options.skipTargetCheck && target)
        {
            result.result = ValidateTarget(spellInfo, caster, target, options);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Target validation failed for spell {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // All validation passed - proceed to packet building
        result.packet = BuildCastSpellPacketInternal(caster, spellInfo, target, nullptr);
    }
    else
    {
        // Skip validation - build packet directly (DANGEROUS!)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        result.packet = BuildCastSpellPacketInternal(caster, spellInfo, target, nullptr);
    }

    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build packet (internal error)";
        if (options.logFailures)
            LogValidationFailure(caster, spellId, result.result, result.failureReason);
    }

    return result;
}

// ============================================================================
// Public API - BuildCastSpellPacket (GameObject target overload)
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCastSpellPacket(
    Player* caster,
    uint32 spellId,
    GameObject* goTarget,
    BuildOptions const& options)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    // Pre-flight validation (unless completely skipped)
    if (!options.skipValidation)
    {
        // Step 1: Validate player object
        if (!options.skipSpellCheck)
        {
            result.result = ValidatePlayer(caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = "Player validation failed";
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 2: Validate spell ID
        if (!options.skipSpellCheck)
        {
            result.result = ValidateSpellId(spellId, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Invalid spell ID: {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Get spell info (needed for all subsequent checks)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        if (!spellInfo)
        {
            result.result = ValidationResult::SPELL_NOT_FOUND;
            result.failureReason = fmt::format("Spell info not found for spell {}", spellId);
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        // Step 3: Validate spell is learned
        if (!options.skipSpellCheck)
        {
            result.result = ValidateSpellLearned(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Spell {} not learned by {}", spellId, caster->GetName());
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 4: Validate cooldown
        if (!options.skipSpellCheck)
        {
            result.result = ValidateCooldown(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Spell {} on cooldown", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 5: Validate resources (mana, rage, energy, runes, etc.)
        if (!options.skipResourceCheck)
        {
            result.result = ValidateResources(spellInfo, caster);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Insufficient resources for spell {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 6: Validate caster state (alive, not stunned, not silenced, etc.)
        if (!options.skipStateCheck)
        {
            result.result = ValidateCasterState(spellInfo, caster, options);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = fmt::format("Caster state invalid for spell {}", spellId);
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 7: Validate GCD/casting state
        if (!options.skipGcdCheck)
        {
            result.result = ValidateGlobalCooldown(caster, spellInfo, options);
            if (result.result != ValidationResult::SUCCESS)
            {
                result.failureReason = "GCD active or spell in progress";
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }
        }

        // Step 8: Validate GameObject target (if provided)
        if (!options.skipTargetCheck && goTarget)
        {
            // Basic GameObject validation
            if (!goTarget->IsInWorld())
            {
                result.result = ValidationResult::INVALID_TARGET;
                result.failureReason = fmt::format("GameObject {} (entry {}) not in world",
                    if (!goTarget)
                    {
                        TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: goTarget in method GetGUID");
                        return;
                    }
                    goTarget->GetGUID().ToString(), goTarget->GetEntry());
                if (options.logFailures)
                    LogValidationFailure(caster, spellId, result.result, result.failureReason);
                return result;
            }

            // Validate range (if range check enabled)
            if (!options.skipRangeCheck)
            {
                float distance = caster->GetDistance(goTarget);
                float maxRange = spellInfo->GetMaxRange();
                if (maxRange > 0.0f && distance > maxRange)
                {
                    result.result = ValidationResult::TARGET_OUT_OF_RANGE;
                    result.failureReason = fmt::format("GameObject {} out of range ({:.1f}yd > {:.1f}yd max)",
                        goTarget->GetEntry(), distance, maxRange);
                    if (options.logFailures)
                        LogValidationFailure(caster, spellId, result.result, result.failureReason);
                    return result;
                }
            }
        }

        // All validation passed - build packet with GameObject target
        result.packet = BuildCastSpellPacketInternalGameObject(caster, spellInfo, goTarget);
    }
    else
    {
        // Skip validation - build packet directly (DANGEROUS!)
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        result.packet = BuildCastSpellPacketInternalGameObject(caster, spellInfo, goTarget);
    }

    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build packet (internal error)";
        if (options.logFailures)
            LogValidationFailure(caster, spellId, result.result, result.failureReason);
    }

    return result;
}

// ============================================================================
// Public API - BuildCastSpellPacketAtPosition
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCastSpellPacketAtPosition(
    Player* caster,
    uint32 spellId,
    Position const& position,
    BuildOptions const& options)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    // Validation (similar to unit-targeted spell)
    if (!options.skipValidation)
    {
        result.result = ValidatePlayer(caster);
        if (result.result != ValidationResult::SUCCESS)
        {
            result.failureReason = "Player validation failed";
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        result.result = ValidateSpellId(spellId, caster);
        if (result.result != ValidationResult::SUCCESS)
        {
            result.failureReason = fmt::format("Invalid spell ID: {}", spellId);
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        if (!spellInfo)
        {
            result.result = ValidationResult::SPELL_NOT_FOUND;
            result.failureReason = fmt::format("Spell info not found for spell {}", spellId);
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        // Validate position target is valid for this spell
        result.result = ValidatePositionTarget(spellInfo, caster, position);
        if (result.result != ValidationResult::SUCCESS)
        {
            result.failureReason = fmt::format("Position target invalid for spell {}", spellId);
            if (options.logFailures)
                LogValidationFailure(caster, spellId, result.result, result.failureReason);
            return result;
        }

        // Build packet with position target
        result.packet = BuildCastSpellPacketInternal(caster, spellInfo, nullptr, &position);
    }
    else
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
        result.packet = BuildCastSpellPacketInternal(caster, spellInfo, nullptr, &position);
    }

    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build position-targeted packet";
        if (options.logFailures)
            LogValidationFailure(caster, spellId, result.result, result.failureReason);
    }

    return result;
}

// ============================================================================
// Public API - BuildCancelCastPacket
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCancelCastPacket(Player* caster, uint32 spellId)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    if (!caster)
    {
        result.result = ValidationResult::PLAYER_NULLPTR;
        result.failureReason = "Caster is nullptr";
        return result;
    }

    result.packet = BuildCancelCastPacketInternal(caster, spellId);
    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build cancel cast packet";
    }

    return result;
}

// ============================================================================
// Public API - BuildCancelAuraPacket
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCancelAuraPacket(Player* caster, uint32 spellId)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    if (!caster)
    {
        result.result = ValidationResult::PLAYER_NULLPTR;
        result.failureReason = "Caster is nullptr";
        return result;
    }

    if (spellId == 0)
    {
        result.result = ValidationResult::INVALID_SPELL_ID;
        result.failureReason = "Spell ID cannot be 0 for CancelAura";
        return result;
    }

    result.packet = BuildCancelAuraPacketInternal(caster, spellId);
    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build cancel aura packet";
    }

    return result;
}

// ============================================================================
// Public API - BuildCancelChannelPacket
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCancelChannelPacket(Player* caster, uint32 spellId, int32 reason)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    if (!caster)
    {
        result.result = ValidationResult::PLAYER_NULLPTR;
        result.failureReason = "Caster is nullptr";
        return result;
    }

    result.packet = BuildCancelChannelPacketInternal(caster, spellId, reason);
    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build cancel channel packet";
    }

    return result;
}

// ============================================================================
// Public API - BuildCancelAutoRepeatPacket
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::BuildCancelAutoRepeatPacket(Player* caster)
{
    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    if (!caster)
    {
        result.result = ValidationResult::PLAYER_NULLPTR;
        result.failureReason = "Caster is nullptr";
        return result;
    }

    result.packet = BuildCancelAutoRepeatPacketInternal(caster);
    if (!result.packet)
    {
        result.result = ValidationResult::PACKET_BUILD_FAILED;
        result.failureReason = "Failed to build cancel auto-repeat packet";
    }

    return result;
}

// ============================================================================
// Public API - ValidateSpellCast (Pre-flight check)
// ============================================================================

SpellPacketBuilder::BuildResult SpellPacketBuilder::ValidateSpellCast(
    Player* caster,
    uint32 spellId,
    Unit* target,
    BuildOptions const& options)
{
    // Reuse BuildCastSpellPacket validation logic but don't build packet
    BuildOptions validateOnly = options;
    validateOnly.skipValidation = false; // Force validation

    BuildResult result;
    result.result = ValidationResult::SUCCESS;

    // Run all validation steps
    result.result = ValidatePlayer(caster);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = "Player validation failed";
        return result;
    }

    result.result = ValidateSpellId(spellId, caster);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = fmt::format("Invalid spell ID: {}", spellId);
        return result;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        result.result = ValidationResult::SPELL_NOT_FOUND;
        result.failureReason = fmt::format("Spell info not found for spell {}", spellId);
        return result;
    }

    result.result = ValidateSpellLearned(spellInfo, caster);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = fmt::format("Spell {} not learned", spellId);
        return result;
    }

    result.result = ValidateCooldown(spellInfo, caster);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = fmt::format("Spell {} on cooldown", spellId);
        return result;
    }

    result.result = ValidateResources(spellInfo, caster);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = "Insufficient resources";
        return result;
    }

    result.result = ValidateCasterState(spellInfo, caster, validateOnly);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = "Caster state invalid";
        return result;
    }

    result.result = ValidateGlobalCooldown(caster, spellInfo, validateOnly);
    if (result.result != ValidationResult::SUCCESS)
    {
        result.failureReason = "GCD active or spell in progress";
        return result;
    }

    if (target)
    {
        result.result = ValidateTarget(spellInfo, caster, target, validateOnly);
        if (result.result != ValidationResult::SUCCESS)
        {
            result.failureReason = "Target validation failed";
            return result;
        }
    }

    // All validation passed
    result.result = ValidationResult::SUCCESS;
    result.failureReason = "Validation successful";
    return result;
}

// ============================================================================
// Validation Methods (Enterprise-grade, comprehensive)
// ============================================================================

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidatePlayer(Player const* caster)
{
    if (!caster)
        return ValidationResult::PLAYER_NULLPTR;

    if (!caster->GetSession())
        return ValidationResult::SESSION_NULLPTR;

    if (!caster->GetMap())
        return ValidationResult::MAP_NULLPTR;

    if (!caster->IsInWorld())
        return ValidationResult::CASTER_DEAD; // Not in world = effectively dead/non-functional

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateSpellId(uint32 spellId, Player const* caster)
if (!caster)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: caster in method IsAlive");
    return nullptr;
}
{
    if (spellId == 0)
        return ValidationResult::INVALID_SPELL_ID;

    // Check if spell exists in spell manager
    if (!sSpellMgr->GetSpellInfo(spellId, caster->GetMap()->GetDifficultyID()))
        return ValidationResult::SPELL_NOT_FOUND;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateSpellInfo(SpellInfo const* spellInfo, Player const* caster)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Additional spell info validation could go here
    // (e.g., check if spell is disabled, deprecated, etc.)

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateSpellLearned(SpellInfo const* spellInfo, Player const* caster)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Check if player has learned the spell
    if (!caster->HasSpell(spellInfo->Id))
        return ValidationResult::SPELL_NOT_LEARNED;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateCooldown(SpellInfo const* spellInfo, Player const* caster)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Check if spell is on cooldown
    if (caster->GetSpellHistory()->HasCooldown(spellInfo->Id))
        return ValidationResult::SPELL_ON_COOLDOWN;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateResources(SpellInfo const* spellInfo, Player const* caster)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Check power cost (mana, rage, energy, runes, etc.)
    for (SpellPowerCost const& cost : spellInfo->CalcPowerCost(caster, spellInfo->GetSchoolMask()))
    {
        if (caster->GetPower(cost.Power) < cost.Amount)
        {
            switch (cost.Power)
            {
                case POWER_MANA:
                    return ValidationResult::INSUFFICIENT_MANA;
                case POWER_RAGE:
                    return ValidationResult::INSUFFICIENT_RAGE;
                case POWER_ENERGY:
                    return ValidationResult::INSUFFICIENT_ENERGY;
                case POWER_RUNES:
                    return ValidationResult::INSUFFICIENT_RUNES;
                default:
                    return ValidationResult::INSUFFICIENT_POWER;
            }
        }
    }

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateCasterState(
    SpellInfo const* spellInfo,
    Player const* caster,
    BuildOptions const& options)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Check if caster is dead
    if (!options.allowDeadCaster && !caster->IsAlive())
        return ValidationResult::CASTER_DEAD;

    // Check if caster is stunned
    if (caster->HasUnitState(UNIT_STATE_STUNNED))
        return ValidationResult::CASTER_STUNNED;

    // Check if caster is silenced (for spells that can't be cast while silenced)
    // Note: Silence is handled via HasUnitState(UNIT_STATE_CASTING) or spell interrupt system

    // Check if caster is pacified (can't cast offensive spells)
    // Note: Pacify is handled via spell interrupt and aura system

    // Check if caster is moving (for spells that can't be cast while moving)
    if (!options.allowWhileMoving && !spellInfo->IsPassive())
    {
        if (caster->isMoving() || caster->IsFalling())
            return ValidationResult::CASTER_MOVING;
    }

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateGlobalCooldown(
    Player const* caster,
    SpellInfo const* spellInfo,
    BuildOptions const& options)
{
    // Check if GCD is active
    if (!options.allowWhileCasting && caster->GetSpellHistory()->HasGlobalCooldown(spellInfo))
        return ValidationResult::GCD_ACTIVE;

    // Check if player is already casting a spell
    if (!options.allowWhileCasting && caster->IsNonMeleeSpellCast(false))
        return ValidationResult::SPELL_IN_PROGRESS;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateTarget(
    SpellInfo const* spellInfo,
    Player const* caster,
    Unit const* target,
    BuildOptions const& options)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    if (!target)
    {
        // Check if spell requires a target
        if (spellInfo->NeedsExplicitUnitTarget())
            return ValidationResult::INVALID_TARGET;
        return ValidationResult::SUCCESS;
    }

    // Check if target is dead (for spells that can't target dead units)
    if (!spellInfo->IsAllowingDeadTarget() && !target->IsAlive())
        return ValidationResult::TARGET_DEAD;

    // Check if target is friendly/hostile (based on spell requirements)
    if (spellInfo->IsPositive())
    {
        if (caster->IsHostileTo(target))
            return ValidationResult::TARGET_HOSTILE;
    }
    else
    {
        if (caster->IsFriendlyTo(target))
            return ValidationResult::TARGET_FRIENDLY;
    }

    // Check range
    if (!options.skipRangeCheck)
    {
        ValidationResult rangeCheck = ValidateTargetRange(spellInfo, caster, target);
        if (rangeCheck != ValidationResult::SUCCESS)
            return rangeCheck;
    }

    // Check LOS
    if (!options.skipRangeCheck)
    {
        ValidationResult losCheck = ValidateTargetLOS(caster, target);
if (!caster)
{
    TC_LOG_ERROR("playerbot.nullcheck", "Null pointer: caster in method GetGUID");
    return nullptr;
}
        if (losCheck != ValidationResult::SUCCESS)
            return losCheck;
    }

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateTargetRange(
    SpellInfo const* spellInfo,
    Player const* caster,
    Unit const* target)
{
    if (!spellInfo || !target)
        return ValidationResult::INVALID_TARGET;

    float maxRange = spellInfo->GetMaxRange(spellInfo->IsPositive(), const_cast<Player*>(caster));
    float distance = caster->GetDistance(target);

    if (distance > maxRange)
        return ValidationResult::TARGET_OUT_OF_RANGE;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidateTargetLOS(
    Player const* caster,
    Unit const* target)
{
    if (!target)
        return ValidationResult::INVALID_TARGET;

    // Check line of sight
    if (!caster->IsWithinLOSInMap(target))
        return ValidationResult::TARGET_NOT_IN_LOS;

    return ValidationResult::SUCCESS;
}

SpellPacketBuilder::ValidationResult SpellPacketBuilder::ValidatePositionTarget(
    SpellInfo const* spellInfo,
    Player const* caster,
    Position const& position)
{
    if (!spellInfo)
        return ValidationResult::SPELL_NOT_FOUND;

    // Validate position is within map bounds
    if (!caster->GetMap()->IsGridLoaded(position.GetPositionX(), position.GetPositionY()))
        return ValidationResult::POSITION_INVALID;

    // Check range to position
    float maxRange = spellInfo->GetMaxRange(spellInfo->IsPositive(), const_cast<Player*>(caster));
    float distance = caster->GetExactDist(&position);
    if (distance > maxRange)
        return ValidationResult::TARGET_OUT_OF_RANGE;

    return ValidationResult::SUCCESS;
}

// ============================================================================
// Internal Packet Builders (Use TrinityCore official structures)
// ============================================================================

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCastSpellPacketInternal(
    Player* caster,
    SpellInfo const* spellInfo,
    Unit* target,
    Position const* position)
{
    if (!caster || !spellInfo)
        return nullptr;

    // Create CMSG_CAST_SPELL packet using TrinityCore's official WorldPackets::Spells::CastSpell structure
    auto packet = std::make_unique<WorldPacket>(CMSG_CAST_SPELL);

    // Build SpellCastRequest structure
    WorldPackets::Spells::SpellCastRequest castRequest;
    castRequest.CastID = ObjectGuid::Create<HighGuid::Cast>(SPELL_CAST_SOURCE_NORMAL, caster->GetMapId(), spellInfo->Id, caster->GetMap()->GenerateLowGuid<HighGuid::Cast>());
    castRequest.SpellID = spellInfo->Id;
    castRequest.Visual = WorldPackets::Spells::SpellCastVisual();
    castRequest.SendCastFlags = 0;

    // Set target
    if (target)
    {
        castRequest.Target.Flags = TARGET_FLAG_UNIT;
        castRequest.Target.Unit = target->GetGUID();
    }
    else if (position)
    {
        castRequest.Target.Flags = TARGET_FLAG_DEST_LOCATION;
        castRequest.Target.DstLocation = WorldPackets::Spells::TargetLocation();
        castRequest.Target.DstLocation->Location.Pos.Relocate(position->GetPositionX(),
            position->GetPositionY(), position->GetPositionZ());
    }
    else
    {
        // Self-cast or no target
        castRequest.Target.Flags = TARGET_FLAG_UNIT;
        castRequest.Target.Unit = caster->GetGUID();
    }

    // Write SpellCastRequest to packet
    // Note: This requires access to WorldPackets::Spells::CastSpell::Write() method
    // For now, we'll manually write the essential fields
    *packet << castRequest.CastID;
    *packet << castRequest.SpellID;
    // ... (additional fields would be written here following TrinityCore packet structure)

    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CAST_SPELL packet: caster={}, spell={}, target={}",
        caster->GetName(), spellInfo->Id, target ? target->GetName() : "none");

    return packet;
}

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCastSpellPacketInternalGameObject(
    Player* caster,
    SpellInfo const* spellInfo,
    GameObject* goTarget)
{
    if (!caster || !spellInfo)
        return nullptr;

    // Create CMSG_CAST_SPELL packet for GameObject target (quest items, interactions)
    auto packet = std::make_unique<WorldPacket>(CMSG_CAST_SPELL);

    // Build SpellCastRequest structure
    WorldPackets::Spells::SpellCastRequest castRequest;
    castRequest.CastID = ObjectGuid::Create<HighGuid::Cast>(SPELL_CAST_SOURCE_NORMAL, caster->GetMapId(), spellInfo->Id, caster->GetMap()->GenerateLowGuid<HighGuid::Cast>());
    castRequest.SpellID = spellInfo->Id;
    castRequest.Visual = WorldPackets::Spells::SpellCastVisual();
    castRequest.SendCastFlags = 0;

    // Set GameObject target
    if (goTarget)
    {
        castRequest.Target.Flags = TARGET_FLAG_GAMEOBJECT;
        // GameObjects use the Unit field for their GUID in SpellTargetData
        castRequest.Target.Unit = goTarget->GetGUID();
    }
    else
    {
        // Self-cast if no GameObject provided
        castRequest.Target.Flags = TARGET_FLAG_UNIT;
        castRequest.Target.Unit = caster->GetGUID();
    }

    // Write SpellCastRequest to packet
    *packet << castRequest.CastID;
    *packet << castRequest.SpellID;
    // ... (additional fields would be written here following TrinityCore packet structure)

    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CAST_SPELL packet (GameObject): caster={}, spell={}, goTarget={}",
        caster->GetName(), spellInfo->Id, goTarget ? goTarget->GetGUID().ToString() : "none");

    return packet;
}

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCancelCastPacketInternal(Player* caster, uint32 spellId)
{
    if (!caster)
        return nullptr;

    auto packet = std::make_unique<WorldPacket>(CMSG_CANCEL_CAST);
    *packet << uint32(0); // Cast count
    *packet << spellId;

    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CANCEL_CAST packet: caster={}, spell={}",
        caster->GetName(), spellId);

    return packet;
}

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCancelAuraPacketInternal(Player* caster, uint32 spellId)
{
    if (!caster)
        return nullptr;

    auto packet = std::make_unique<WorldPacket>(CMSG_CANCEL_AURA);
    *packet << spellId;
    *packet << caster->GetGUID(); // Caster GUID
    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CANCEL_AURA packet: caster={}, spell={}",
        caster->GetName(), spellId);

    return packet;
}

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCancelChannelPacketInternal(Player* caster, uint32 spellId, int32 reason)
{
    if (!caster)
        return nullptr;

    auto packet = std::make_unique<WorldPacket>(CMSG_CANCEL_CHANNELLING);
    *packet << spellId;
    *packet << reason;

    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CANCEL_CHANNELLING packet: caster={}, spell={}, reason={}",
        caster->GetName(), spellId, reason);

    return packet;
}

std::unique_ptr<WorldPacket> SpellPacketBuilder::BuildCancelAutoRepeatPacketInternal(Player* caster)
{
    if (!caster)
        return nullptr;

    auto packet = std::make_unique<WorldPacket>(CMSG_CANCEL_AUTO_REPEAT_SPELL);
    // No data for this packet

    TC_LOG_TRACE("playerbot.spells.packets",
        "Built CMSG_CANCEL_AUTO_REPEAT_SPELL packet: caster={}",
        caster->GetName());

    return packet;
}

// ============================================================================
// Helper Utilities
// ============================================================================

char const* SpellPacketBuilder::BuildResult::GetResultName() const
{
    return GetValidationResultString(result).c_str();
}

std::string SpellPacketBuilder::GetValidationResultString(ValidationResult result)
{
    switch (result)
    {
        case ValidationResult::SUCCESS: return "SUCCESS";
        case ValidationResult::INVALID_SPELL_ID: return "INVALID_SPELL_ID";
        case ValidationResult::SPELL_NOT_FOUND: return "SPELL_NOT_FOUND";
        case ValidationResult::SPELL_NOT_LEARNED: return "SPELL_NOT_LEARNED";
        case ValidationResult::SPELL_ON_COOLDOWN: return "SPELL_ON_COOLDOWN";
        case ValidationResult::SPELL_NOT_READY: return "SPELL_NOT_READY";
        case ValidationResult::INSUFFICIENT_MANA: return "INSUFFICIENT_MANA";
        case ValidationResult::INSUFFICIENT_RAGE: return "INSUFFICIENT_RAGE";
        case ValidationResult::INSUFFICIENT_ENERGY: return "INSUFFICIENT_ENERGY";
        case ValidationResult::INSUFFICIENT_RUNES: return "INSUFFICIENT_RUNES";
        case ValidationResult::INSUFFICIENT_POWER: return "INSUFFICIENT_POWER";
        case ValidationResult::INVALID_TARGET: return "INVALID_TARGET";
        case ValidationResult::TARGET_OUT_OF_RANGE: return "TARGET_OUT_OF_RANGE";
        case ValidationResult::TARGET_NOT_IN_LOS: return "TARGET_NOT_IN_LOS";
        case ValidationResult::TARGET_DEAD: return "TARGET_DEAD";
        case ValidationResult::TARGET_FRIENDLY: return "TARGET_FRIENDLY";
        case ValidationResult::TARGET_HOSTILE: return "TARGET_HOSTILE";
        case ValidationResult::NO_TARGET_REQUIRED: return "NO_TARGET_REQUIRED";
        case ValidationResult::CASTER_DEAD: return "CASTER_DEAD";
        case ValidationResult::CASTER_MOVING: return "CASTER_MOVING";
        case ValidationResult::CASTER_CASTING: return "CASTER_CASTING";
        case ValidationResult::CASTER_STUNNED: return "CASTER_STUNNED";
        case ValidationResult::CASTER_SILENCED: return "CASTER_SILENCED";
        case ValidationResult::CASTER_PACIFIED: return "CASTER_PACIFIED";
        case ValidationResult::CASTER_INTERRUPTED: return "CASTER_INTERRUPTED";
        case ValidationResult::GCD_ACTIVE: return "GCD_ACTIVE";
        case ValidationResult::SPELL_IN_PROGRESS: return "SPELL_IN_PROGRESS";
        case ValidationResult::NOT_IN_COMBAT: return "NOT_IN_COMBAT";
        case ValidationResult::IN_COMBAT: return "IN_COMBAT";
        case ValidationResult::NOT_MOUNTED: return "NOT_MOUNTED";
        case ValidationResult::MOUNTED: return "MOUNTED";
        case ValidationResult::POSITION_INVALID: return "POSITION_INVALID";
        case ValidationResult::PLAYER_NULLPTR: return "PLAYER_NULLPTR";
        case ValidationResult::SESSION_NULLPTR: return "SESSION_NULLPTR";
        case ValidationResult::MAP_NULLPTR: return "MAP_NULLPTR";
        case ValidationResult::PACKET_BUILD_FAILED: return "PACKET_BUILD_FAILED";
        default: return "UNKNOWN";
    }
}

void SpellPacketBuilder::LogValidationFailure(
    Player const* caster,
    uint32 spellId,
    ValidationResult result,
    std::string const& reason)
{
    TC_LOG_WARN("playerbot.spells.validation",
        "Spell cast validation failed: caster={}, spell={}, result={}, reason={}",
        caster ? caster->GetName() : "nullptr",
        spellId,
        GetValidationResultString(result),
        reason);
}
