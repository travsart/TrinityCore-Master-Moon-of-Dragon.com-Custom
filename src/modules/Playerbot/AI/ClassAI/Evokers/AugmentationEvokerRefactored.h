/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
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

#ifndef PLAYERBOT_AUGMENTATIONEVOKERREFACTORED_H
#define PLAYERBOT_AUGMENTATIONEVOKERREFACTORED_H

#include "../CombatSpecializationTemplates.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <unordered_map>
#include <vector>
#include "Log.h"
#include "EvokerSpecialization.h"

namespace Playerbot
{

// WoW 11.2 (The War Within) - Augmentation Evoker Spell IDs
constexpr uint32 AUG_EBON_MIGHT = 395152;              // Primary buff - damage amplification
constexpr uint32 AUG_PRESCIENCE = 409311;              // Critical strike buff
constexpr uint32 AUG_BLISTERING_SCALES = 360827;       // Defensive buff
constexpr uint32 AUG_BREATH_OF_EONS = 403631;          // Empowered - group damage boost
constexpr uint32 AUG_ERUPTION = 395160;                // Essence spender
constexpr uint32 AUG_UPHEAVAL = 396286;                // Earth magic damage
constexpr uint32 AUG_AZURE_STRIKE = 362969;            // Basic attack
constexpr uint32 AUG_LIVING_FLAME = 361469;            // Damage/heal hybrid
constexpr uint32 AUG_FIRE_BREATH = 382266;             // Empowered fire damage
constexpr uint32 AUG_DISINTEGRATE = 356995;            // Channel damage
constexpr uint32 AUG_DEEP_BREATH = 357210;             // Flying AOE
constexpr uint32 AUG_TIP_THE_SCALES = 370553;          // Instant empower
constexpr uint32 AUG_OBSIDIAN_SCALES = 363916;         // Defensive cooldown
constexpr uint32 AUG_RENEWING_BLAZE = 374348;          // Self-heal
constexpr uint32 AUG_VERDANT_EMBRACE = 360995;         // Ally heal
constexpr uint32 AUG_REACTIVE_HIDE = 410256;           // Shield buff
constexpr uint32 AUG_SPATIAL_PARADOX = 406732;         // Utility cooldown
constexpr uint32 AUG_TIME_SKIP = 404977;               // Reset cooldowns
constexpr uint32 AUG_TREMBLING_EARTH = 409392;         // Utility damage

// Mana and Essence resource for Augmentation
struct ManaEssenceResourceAug
{
    uint32 mana = 0;
    uint32 maxMana = 100;
    uint32 essence = 0;
    uint32 maxEssence = 6;

    void Initialize(Player* bot)
    {
        if (!bot)
            return;

        mana = bot->GetPower(POWER_MANA);
        maxMana = bot->GetMaxPower(POWER_MANA);
        essence = bot->GetPower(POWER_ESSENCE);
        maxEssence = bot->GetMaxPower(POWER_ESSENCE);
    }

    void Update(Player* bot)
    {
        if (!bot)
            return;

        mana = bot->GetPower(POWER_MANA);
        essence = bot->GetPower(POWER_ESSENCE);
    }

    [[nodiscard]] bool HasMana(uint32 amount) const { return mana >= amount; }
    [[nodiscard]] bool HasEssence(uint32 amount) const { return essence >= amount; }
    [[nodiscard]] uint32 GetManaPercent() const { return maxMana > 0 ? (mana * 100) / maxMana : 0; }
    [[nodiscard]] uint32 GetAvailableEssence() const { return essence; }
    [[nodiscard]] uint32 GetMaxEssence() const { return maxEssence; }

    bool Consume(uint32 essenceCost)
    {
        if (!HasEssence(essenceCost))
            return false;

        essence -= essenceCost;
        return true;
    }

    void Regenerate(uint32 amount)
    {
        essence = std::min(essence + amount, maxEssence);
    }

    [[nodiscard]] bool available() const { return essence > 0; }
};

// Ebon Might buff tracker
struct AugmentationEbonMightInfo
{
    ::Unit* target = nullptr;
    uint32 expiresAt = 0;
    uint8 stacks = 0;

    [[nodiscard]] bool IsActive() const
    {
        return target != nullptr && getMSTime() < expiresAt && stacks > 0;
    }

    [[nodiscard]] uint32 TimeRemaining() const
    {
        uint32 now = getMSTime();
        return expiresAt > now ? expiresAt - now : 0;
    }

    void Apply(::Unit* tgt, uint32 duration = 30000)
    {
        target = tgt;
        expiresAt = getMSTime() + duration;
        stacks = 1;
    }

    void Expire()
    {
        target = nullptr;
        expiresAt = 0;
        stacks = 0;
    }
};

// Prescience buff tracker
struct AugmentationPrescienceInfo
{
    ::Unit* target = nullptr;
    uint32 expiresAt = 0;

    [[nodiscard]] bool IsActive() const
    {
        return target != nullptr && getMSTime() < expiresAt;
    }

    [[nodiscard]] uint32 TimeRemaining() const
    {
        uint32 now = getMSTime();
        return expiresAt > now ? expiresAt - now : 0;
    }

    void Apply(::Unit* tgt, uint32 duration = 18000)
    {
        target = tgt;
        expiresAt = getMSTime() + duration;
    }

    void Expire()
    {
        target = nullptr;
        expiresAt = 0;
    }
};

// Buff distribution manager
class AugmentationBuffDistributor
{
public:
    AugmentationBuffDistributor()
        : _lastEbonMightTime(0), _lastPrescienceTime(0), _lastBuffRefreshTime(0),
          _maxEbonMightTargets(4), _maxPrescienceTargets(2)
    {
    }

    void Update()
    {
        uint32 now = getMSTime();

        // Update Ebon Might trackers
        for (auto& ebonMight : _ebonMightTrackers)
        {
            if (ebonMight.IsActive() && ebonMight.TimeRemaining() == 0)
                ebonMight.Expire();
        }

        // Update Prescience trackers
        for (auto& prescience : _prescienceTrackers)
        {
            if (prescience.IsActive() && prescience.TimeRemaining() == 0)
                prescience.Expire();
        }

        // Remove expired entries
        _ebonMightTrackers.erase(
            std::remove_if(_ebonMightTrackers.begin(), _ebonMightTrackers.end(),
                [](const AugmentationEbonMightInfo& info) { return !info.IsActive(); }),
            _ebonMightTrackers.end());

        _prescienceTrackers.erase(
            std::remove_if(_prescienceTrackers.begin(), _prescienceTrackers.end(),
                [](const AugmentationPrescienceInfo& info) { return !info.IsActive(); }),
            _prescienceTrackers.end());
    }

    [[nodiscard]] bool NeedsEbonMight(::Unit* target) const
    {
        for (const auto& tracker : _ebonMightTrackers)
        {
            if (tracker.target == target && tracker.IsActive())
                return false;
        }
        return _ebonMightTrackers.size() < _maxEbonMightTargets;
    }

    [[nodiscard]] bool NeedsPrescience(::Unit* target) const
    {
        for (const auto& tracker : _prescienceTrackers)
        {
            if (tracker.target == target && tracker.IsActive())
                return false;
        }
        return _prescienceTrackers.size() < _maxPrescienceTargets;
    }

    void ApplyEbonMight(::Unit* target)
    {
        AugmentationEbonMightInfo info;
        info.Apply(target);
        _ebonMightTrackers.push_back(info);
        _lastEbonMightTime = getMSTime();
    }

    void ApplyPrescience(::Unit* target)
    {
        AugmentationPrescienceInfo info;
        info.Apply(target);
        _prescienceTrackers.push_back(info);
        _lastPrescienceTime = getMSTime();
    }

    [[nodiscard]] bool ShouldRefreshBuffs() const
    {
        return getMSTime() - _lastBuffRefreshTime > 5000; // Every 5 seconds
    }

    void MarkBuffRefreshed()
    {
        _lastBuffRefreshTime = getMSTime();
    }

    [[nodiscard]] uint32 GetActiveEbonMightCount() const
    {
        return static_cast<uint32>(std::count_if(_ebonMightTrackers.begin(), _ebonMightTrackers.end(),
            [](const AugmentationEbonMightInfo& info) { return info.IsActive(); }));
    }

    [[nodiscard]] uint32 GetActivePrescienceCount() const
    {
        return static_cast<uint32>(std::count_if(_prescienceTrackers.begin(), _prescienceTrackers.end(),
            [](const AugmentationPrescienceInfo& info) { return info.IsActive(); }));
    }

private:
    std::vector<AugmentationEbonMightInfo> _ebonMightTrackers;
    std::vector<AugmentationPrescienceInfo> _prescienceTrackers;
    uint32 _lastEbonMightTime;
    uint32 _lastPrescienceTime;
    uint32 _lastBuffRefreshTime;
    uint32 _maxEbonMightTargets;
    uint32 _maxPrescienceTargets;
};

/**
 * Augmentation Evoker - Support Specialization (WoW 11.2)
 *
 * Role: Hybrid Support DPS
 * Resource: Mana + Essence (dual resource)
 * Range: 25-30 yards
 *
 * Playstyle: Buff-focused support that amplifies ally damage while contributing
 *           moderate DPS through empowered abilities and essence management.
 *
 * Core Mechanics:
 * - Ebon Might: Primary buff - increases ally damage (4 max targets, 30s duration)
 * - Prescience: Crit buff for top DPS (2 max targets, 18s duration)
 * - Breath of Eons: Empowered group damage boost
 * - Essence Management: Balance essence for buffs vs. damage
 * - Buff Distribution: Prioritize highest damage dealers
 *
 * Rotation Priority:
 * 1. Maintain Ebon Might on top 4 damage dealers
 * 2. Maintain Prescience on top 2 damage dealers
 * 3. Use Breath of Eons when essence is high
 * 4. Fill with damage abilities (Living Flame, Eruption, Upheaval)
 * 5. Refresh buffs before expiration (30% threshold)
 */
class AugmentationEvokerRefactored : public HybridSpecialization<ManaEssenceResourceAug>
{
public:
    explicit AugmentationEvokerRefactored(Player* bot)
        : HybridSpecialization<ManaEssenceResourceAug>(bot, SpecializationRole::HYBRID),
          _buffDistributor(),
          _prioritizeBuffs(true),
          _lastBreathOfEonsTime(0),
          _combatTime(0)
    {
        TC_LOG_DEBUG("playerbot", "AugmentationEvokerRefactored: Initialized for bot {}", bot->GetName());
    }

    ~AugmentationEvokerRefactored() override = default;

protected:
    // Core rotation override
    void UpdateRotation(::Unit* target) override
    {
        if (!target || !_bot)
            return;

        // Update resource state
        _resource.Update(_bot);

        // Update buff tracking
        _buffDistributor.Update();

        // Update combat time
        _combatTime = getMSTime();

        // Priority rotation
        ExecuteAugmentationRotation(target);
    }

    void ExecuteAugmentationRotation(::Unit* target)
    {
        // 1. Emergency support
        if (_bot->GetHealthPct() < 30.0f)
        {
            if (CastDefensive())
                return;
        }

        // 2. Buff distribution (highest priority)
        if (_prioritizeBuffs)
        {
            if (DistributeBuffs())
                return;
        }

        // 3. Refresh expiring buffs
        if (_buffDistributor.ShouldRefreshBuffs())
        {
            if (RefreshBuffs())
            {
                _buffDistributor.MarkBuffRefreshed();
                return;
            }
        }

        // 4. Empowered Breath of Eons (burst window)
        if (ShouldUseBreathOfEons())
        {
            if (CastBreathOfEons(target))
            {
                _lastBreathOfEonsTime = getMSTime();
                return;
            }
        }

        // 5. Damage contribution
        ContributeDamage(target);
    }

    bool DistributeBuffs()
    {
        std::vector<::Unit*> damageDealer = GetTopDamageDealer(4);

        // Apply Ebon Might to top damage dealers
        for (::Unit* ally : damageDealer)
        {
            if (_buffDistributor.NeedsEbonMight(ally) && _resource.HasEssence(2))
            {
                if (CastEbonMight(ally))
                    return true;
            }
        }

        // Apply Prescience to top 2 damage dealers
        std::vector<::Unit*> topTwo = GetTopDamageDealer(2);
        for (::Unit* ally : topTwo)
        {
            if (_buffDistributor.NeedsPrescience(ally) && _resource.HasEssence(2))
            {
                if (CastPrescience(ally))
                    return true;
            }
        }

        return false;
    }

    bool RefreshBuffs()
    {
        // Simplified buff refresh - reapply to current targets
        return DistributeBuffs();
    }

    bool CastEbonMight(::Unit* target)
    {
        if (!HasSpell(AUG_EBON_MIGHT) || !_resource.HasEssence(2))
            return false;

        if (CastSpell(AUG_EBON_MIGHT, target))
        {
            _resource.Consume(2);
            _buffDistributor.ApplyEbonMight(target);
            TC_LOG_TRACE("playerbot.augmentation", "AugmentationEvoker {}: Cast Ebon Might on {}",
                _bot->GetName(), target->GetName());
            return true;
        }

        return false;
    }

    bool CastPrescience(::Unit* target)
    {
        if (!HasSpell(AUG_PRESCIENCE) || !_resource.HasEssence(2))
            return false;

        if (CastSpell(AUG_PRESCIENCE, target))
        {
            _resource.Consume(2);
            _buffDistributor.ApplyPrescience(target);
            TC_LOG_TRACE("playerbot.augmentation", "AugmentationEvoker {}: Cast Prescience on {}",
                _bot->GetName(), target->GetName());
            return true;
        }

        return false;
    }

    bool ShouldUseBreathOfEons()
    {
        return _resource.HasEssence(4) &&
               HasSpell(AUG_BREATH_OF_EONS) &&
               (getMSTime() - _lastBreathOfEonsTime > 30000); // 30 second cooldown
    }

    bool CastBreathOfEons(::Unit* target)
    {
        if (!ShouldUseBreathOfEons())
            return false;

        if (CastSpell(AUG_BREATH_OF_EONS, target))
        {
            _resource.Consume(4);
            TC_LOG_TRACE("playerbot.augmentation", "AugmentationEvoker {}: Cast Breath of Eons",
                _bot->GetName());
            return true;
        }

        return false;
    }

    void ContributeDamage(::Unit* target)
    {
        if (!target)
            return;

        // Priority: Eruption > Upheaval > Living Flame > Azure Strike
        if (HasSpell(AUG_ERUPTION) && _resource.HasEssence(3))
        {
            if (CastSpell(AUG_ERUPTION, target))
            {
                _resource.Consume(3);
                return;
            }
        }

        if (HasSpell(AUG_UPHEAVAL) && _resource.HasEssence(2))
        {
            if (CastSpell(AUG_UPHEAVAL, target))
            {
                _resource.Consume(2);
                return;
            }
        }

        if (HasSpell(AUG_LIVING_FLAME) && _resource.HasEssence(2))
        {
            if (CastSpell(AUG_LIVING_FLAME, target))
            {
                _resource.Consume(2);
                return;
            }
        }

        // Filler
        if (HasSpell(AUG_AZURE_STRIKE) && _resource.HasEssence(2))
        {
            CastSpell(AUG_AZURE_STRIKE, target);
            _resource.Consume(2);
        }
    }

    bool CastDefensive()
    {
        if (HasSpell(AUG_OBSIDIAN_SCALES) && !IsOnCooldown(AUG_OBSIDIAN_SCALES))
        {
            return CastSpell(AUG_OBSIDIAN_SCALES);
        }

        if (HasSpell(AUG_RENEWING_BLAZE) && !IsOnCooldown(AUG_RENEWING_BLAZE))
        {
            return CastSpell(AUG_RENEWING_BLAZE);
        }

        return false;
    }

    std::vector<::Unit*> GetTopDamageDealer(uint32 maxCount)
    {
        std::vector<::Unit*> result;

        if (Group* group = _bot->GetGroup())
        {
            for (GroupReference* groupRef = group->GetFirstMember(); groupRef; groupRef = groupRef->next())
            {
                if (Player* member = groupRef->GetSource())
                {
                    if (member != _bot && _bot->IsWithinDistInMap(member, 30.0f))
                    {
                        result.push_back(member);
                        if (result.size() >= maxCount)
                            break;
                    }
                }
            }
        }

        return result;
    }

    // Utility methods
    [[nodiscard]] float GetOptimalRange(::Unit* /*target*/) const override
    {
        return 25.0f; // Mid-range for buff application
    }

    void OnCombatStart(::Unit* /*target*/) override
    {
        _combatTime = getMSTime();
        _prioritizeBuffs = true;
    }

    void OnCombatEnd() override
    {
        _prioritizeBuffs = false;
        _buffDistributor = AugmentationBuffDistributor();
    }

private:
    AugmentationBuffDistributor _buffDistributor;
    bool _prioritizeBuffs;
    uint32 _lastBreathOfEonsTime;
    uint32 _combatTime;
};

} // namespace Playerbot

#endif // PLAYERBOT_AUGMENTATIONEVOKERREFACTORED_H
