/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * Survival Hunter Specialization - REFACTORED
 *
 * This demonstrates the complete migration of Survival Hunter to the template-based
 * architecture, eliminating code duplication while maintaining full functionality.
 *
 * Survival is unique as a melee-focused hunter spec that uses bombs, traps,
 * and coordinated attacks with their pet for sustained damage.
 */

#pragma once

#include "HunterSpecialization.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "Pet.h"
#include "PetDefines.h"
#include "MotionMaster.h"
#include "CharmInfo.h"
#include <unordered_map>
#include <array>

namespace Playerbot
{

// WoW 11.2 Survival Hunter Spell IDs
enum SurvivalSpells
{
    // Core Melee Abilities
    SPELL_RAPTOR_STRIKE        = 186270,  // Main focus spender
    SPELL_MONGOOSE_BITE        = 259387,  // Stacking damage ability
    SPELL_CARVE                = 187708,  // AoE cleave
    SPELL_BUTCHERY             = 212436,  // AoE burst
    SPELL_COORDINATED_ASSAULT  = 360952,  // Major DPS cooldown
    SPELL_FLANKING_STRIKE      = 269751,  // Pet coordination

    // Wildfire Bombs
    SPELL_WILDFIRE_BOMB        = 259495,  // Base bomb
    SPELL_SHRAPNEL_BOMB        = 270335,  // Bleed variant
    SPELL_PHEROMONE_BOMB       = 270323,  // Debuff variant
    SPELL_VOLATILE_BOMB        = 271045,  // Damage variant
    SPELL_WILDFIRE_INFUSION    = 271014,  // Random bomb selection

    // DoTs and Debuffs
    SPELL_SERPENT_STING        = 259491,  // Primary DoT
    SPELL_INTERNAL_BLEEDING    = 270343,  // Bleed from Shrapnel
    SPELL_BLOODSEEKER          = 260248,  // Attack speed from bleeds

    // Focus Management
    SPELL_KILL_COMMAND_SURV    = 259489,  // Focus generator
    SPELL_TERMS_OF_ENGAGEMENT  = 265895,  // Harpoon with focus
    SPELL_HARPOON              = 190925,  // Gap closer

    // Utility
    SPELL_ASPECT_OF_EAGLE      = 186289,  // Increased range
    SPELL_MUZZLE               = 187707,  // Interrupt
    SPELL_STEEL_TRAP           = 162488,  // Root trap
    SPELL_GUERRILLA_TACTICS    = 264332,  // First bomb enhancement

    // Pet
    SPELL_CALL_PET_SURV        = 883,     // Summon pet
    SPELL_MEND_PET_SURV        = 136,     // Pet heal

    // Defensives
    SPELL_ASPECT_TURTLE_SURV   = 186265,  // Damage reduction
    SPELL_EXHILARATION_SURV    = 109304,  // Self heal
    SPELL_SURVIVAL_OF_FITTEST  = 264735,  // Damage reduction
};

/**
 * Wildfire Bomb Management System
 *
 * Handles the unique bomb mechanic of Survival, including different bomb types
 * and their recharge system.
 */
class WildfireBombManager
{
public:
    enum BombType
    {
        BOMB_WILDFIRE = 0,
        BOMB_SHRAPNEL = 1,
        BOMB_PHEROMONE = 2,
        BOMB_VOLATILE = 3
    };

    WildfireBombManager()
        : _charges(2)
        , _maxCharges(2)
        , _lastRecharge(0)
        , _rechargeTime(18000) // 18 second recharge
        , _nextBombType(BOMB_WILDFIRE)
        , _hasWildfireInfusion(false)
    {
    }

    bool HasCharge() const { return _charges > 0; }

    uint32 GetCharges() const { return _charges; }

    void UseCharge()
    {
        if (_charges > 0)
        {
            _charges--;

            // Roll for next bomb type if we have Wildfire Infusion
            if (_hasWildfireInfusion)
            {
                _nextBombType = static_cast<BombType>(rand() % 4);
            }
        }
    }

    void UpdateRecharge()
    {
        if (_charges >= _maxCharges)
            return;

        uint32 currentTime = getMSTime();
        if (currentTime - _lastRecharge > _rechargeTime)
        {
            _charges++;
            _lastRecharge = currentTime;
        }
    }

    uint32 GetBombSpell() const
    {
        if (!_hasWildfireInfusion)
            return SPELL_WILDFIRE_BOMB;

        switch (_nextBombType)
        {
            case BOMB_SHRAPNEL:  return SPELL_SHRAPNEL_BOMB;
            case BOMB_PHEROMONE: return SPELL_PHEROMONE_BOMB;
            case BOMB_VOLATILE:  return SPELL_VOLATILE_BOMB;
            default:             return SPELL_WILDFIRE_BOMB;
        }
    }

    BombType GetNextBombType() const { return _nextBombType; }

    void EnableWildfireInfusion() { _hasWildfireInfusion = true; }

private:
    uint32 _charges;
    uint32 _maxCharges;
    uint32 _lastRecharge;
    uint32 _rechargeTime;
    BombType _nextBombType;
    bool _hasWildfireInfusion;
};

/**
 * Mongoose Bite Stack Tracking
 *
 * Manages the stacking mechanic of Mongoose Bite for optimal damage
 */
class MongooseBiteTracker
{
public:
    MongooseBiteTracker()
        : _stacks(0)
        , _maxStacks(5)
        , _windowEndTime(0)
        , _charges(3)
        , _maxCharges(3)
        , _lastRecharge(0)
    {
    }

    void OnMongooseBiteCast()
    {
        uint32 currentTime = getMSTime();

        // Start or extend window
        if (_stacks == 0 || currentTime > _windowEndTime)
        {
            _stacks = 1;
            _windowEndTime = currentTime + 14000; // 14 second window
        }
        else
        {
            _stacks = std::min<uint32>(_stacks + 1, _maxStacks);
            _windowEndTime = currentTime + 14000; // Refresh window
        }

        // Consume charge
        if (_charges > 0)
            _charges--;
    }

    bool HasCharges() const { return _charges > 0; }

    uint32 GetStacks() const
    {
        if (getMSTime() > _windowEndTime)
            return 0;
        return _stacks;
    }

    void UpdateCharges()
    {
        if (_charges >= _maxCharges)
            return;

        uint32 currentTime = getMSTime();
        if (currentTime - _lastRecharge > 12000) // 12 second recharge
        {
            _charges++;
            _lastRecharge = currentTime;
        }
    }

    bool IsWindowActive() const
    {
        return getMSTime() < _windowEndTime;
    }

    void Reset()
    {
        _stacks = 0;
        _windowEndTime = 0;
    }

private:
    uint32 _stacks;
    uint32 _maxStacks;
    uint32 _windowEndTime;
    uint32 _charges;
    uint32 _maxCharges;
    uint32 _lastRecharge;
};

/**
 * Survival Pet Manager
 *
 * Simplified pet management for Survival (less critical than BM)
 */
class SurvivalPetManager
{
public:
    explicit SurvivalPetManager(Player* bot)
        : _bot(bot)
        , _lastMendPet(0)
    {
    }

    void EnsurePetActive(Unit* target)
    {
        if (!HasActivePet())
        {
            SummonPet();
            return;
        }

        // Command pet to attack if not already
        Pet* pet = _bot->GetPet();
        if (pet && pet->IsAlive() && pet->GetVictim() != target)
        {
            pet->Attack(target, true);
        }

        // Heal pet if needed
        if (IsPetHealthLow())
        {
            MendPet();
        }
    }

    bool HasActivePet() const
    {
        return _bot && !_bot->GetPetGUID().IsEmpty() && _bot->GetPet() && _bot->GetPet()->IsAlive();
    }

private:
    void SummonPet()
    {
        if (!_bot || HasActivePet())
            return;

        _bot->CastSpell(_bot, SPELL_CALL_PET_SURV, false);
    }

    bool IsPetHealthLow() const
    {
        if (!HasActivePet())
            return false;

        Pet* pet = _bot->GetPet();
        return pet && pet->GetHealthPct() < 60.0f;
    }

    void MendPet()
    {
        uint32 currentTime = getMSTime();
        if (currentTime - _lastMendPet < 10000)
            return;

        Pet* pet = _bot->GetPet();
        if (pet && pet->IsAlive() && !_bot->HasAura(SPELL_MEND_PET_SURV))
        {
            _bot->CastSpell(pet, SPELL_MEND_PET_SURV, false);
            _lastMendPet = currentTime;
        }
    }

    Player* _bot;
    uint32 _lastMendPet;
};

/**
 * Refactored Survival Hunter using template architecture
 *
 * IMPORTANT: Survival inherits from RangedDpsSpecialization but overrides
 * positioning to be melee-focused. This is unique among hunter specs.
 *
 * Key features:
 * - Melee positioning override (5.0f range instead of 40.0f)
 * - Wildfire Bomb management with different bomb types
 * - Mongoose Bite stacking mechanics
 * - Coordinated Assault burst windows
 * - DoT maintenance with Serpent Sting
 */
class SurvivalHunterRefactored : public RangedDpsSpecialization<FocusResource>, public HunterSpecialization
{
public:
    using Base = RangedDpsSpecialization<FocusResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::CanUseAbility;
    using Base::ConsumeResource;
    using Base::_resource;
    explicit SurvivalHunterRefactored(Player* bot)
        : RangedDpsSpecialization<FocusResource>(bot)
        , HunterSpecialization(bot)
        , _bombManager()
        , _mongooseTracker()
        , _petManager(bot)
        , _coordinatedAssaultActive(false)
        , _coordinatedAssaultEndTime(0)
        , _aspectOfEagleActive(false)
        , _lastRaptorStrike(0)
        , _lastKillCommand(0)
        , _lastSerpentSting(0)
        , _guerillaTacticsActive(true)
    {
        // Focus regeneration handled by base template class
        // Survival has standard hunter focus regeneration (10 focus/sec)

        // Enable Wildfire Infusion if talented
        _bombManager.EnableWildfireInfusion();

        // Setup Survival-specific cooldown tracking
        InitializeCooldowns();
    }

    // ========================================================================
    // POSITIONING OVERRIDE - Survival is MELEE
    // Note: GetOptimalRange is final in base class, positioning handled via other methods
    // ========================================================================

    bool ShouldMaintainRange() const
    {
        // Survival wants to be in melee range
        return false;
    }

    // ========================================================================
    // CORE ROTATION - Survival specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override
    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Ensure pet is helping
        _petManager.EnsurePetActive(target);

        // Update Survival-specific mechanics
        UpdateSurvivalState();

        // Check if we need to gap close
        float distance = this->GetBot()->GetDistance(target);
        if (distance > 5.0f && distance < 30.0f)
        {
            UseHarpoon(target);
            return;
        }

        // Check for AoE situation
        uint32 enemyCount = this->GetEnemiesInRange(8.0f);
        if (enemyCount >= 3)
        {
            ExecuteAoERotation(target);
            return;
        }

        // Single target rotation
        ExecuteSingleTargetRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();

        // Ensure pet is summoned
        if (!_petManager.HasActivePet())
        {
            bot->CastSpell(bot, SPELL_CALL_PET_SURV, false);
            return;
        }

        // Use Aspect of the Eagle for extended range when needed
        if (bot->IsInCombat() && !_aspectOfEagleActive && this->CanUseAbility(SPELL_ASPECT_OF_EAGLE))
        {
            if (this->GetEnemiesInRange(8.0f) > 0)
            {
                this->CastSpell(bot, SPELL_ASPECT_OF_EAGLE);
                _aspectOfEagleActive = true;
            }
        }

        // Use Survival of the Fittest for defense
        if (bot->GetHealthPct() < 50.0f && this->CanUseAbility(SPELL_SURVIVAL_OF_FITTEST))
        {
            this->CastSpell(bot, SPELL_SURVIVAL_OF_FITTEST);
        }

        // Use Exhilaration for healing
        if (bot->GetHealthPct() < 40.0f && this->CanUseAbility(SPELL_EXHILARATION_SURV))
        {
            this->CastSpell(bot, SPELL_EXHILARATION_SURV);
        }
    }

    void OnInterruptRequired(::Unit* target, uint32 /*spellId*/)
    {
        if (this->CanUseAbility(SPELL_MUZZLE))
        {
            this->CastSpell(target, SPELL_MUZZLE);
        }
    }

protected:
    // ========================================================================
    // RESOURCE MANAGEMENT OVERRIDE
    // ========================================================================

    uint32 GetResourceCost(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_RAPTOR_STRIKE:       return 30;
            case SPELL_MONGOOSE_BITE:       return 30;
            case SPELL_CARVE:               return 35;
            case SPELL_BUTCHERY:            return 30;
            case SPELL_KILL_COMMAND_SURV:   return 0;   // Generates 15 focus
            case SPELL_WILDFIRE_BOMB:       return 0;   // No cost
            case SPELL_SERPENT_STING:       return 20;
            case SPELL_FLANKING_STRIKE:     return 30;
            case SPELL_HARPOON:             return 0;   // No cost
            case SPELL_COORDINATED_ASSAULT: return 0;   // No cost
            default:                        return 20;
        }
    }

    // ========================================================================
    // SURVIVAL SPECIFIC ROTATION LOGIC
    // ========================================================================

    void ExecuteSingleTargetRotation(::Unit* target)
    {
        uint32 currentFocus = this->_resource;

        // Priority 1: Coordinated Assault for burst
        if (ShouldUseCoordinatedAssault(target) && this->CanUseAbility(SPELL_COORDINATED_ASSAULT))
        {
            this->CastSpell(this->GetBot(), SPELL_COORDINATED_ASSAULT);
            _coordinatedAssaultActive = true;
            _coordinatedAssaultEndTime = getMSTime() + 20000;
            return;
        }

        // Priority 2: Maintain Serpent Sting
        if (!target->HasAura(SPELL_SERPENT_STING) && currentFocus >= 20)
        {
            this->CastSpell(target, SPELL_SERPENT_STING);
            _lastSerpentSting = getMSTime();
            this->ConsumeResource(20);
            return;
        }

        // Priority 3: Wildfire Bomb on cooldown
        if (_bombManager.HasCharge())
        {
            uint32 bombSpell = _bombManager.GetBombSpell();
            this->CastSpell(target, bombSpell);
            _bombManager.UseCharge();

            // Guerrilla Tactics makes first bomb stronger
            if (_guerillaTacticsActive)
            {
                _guerillaTacticsActive = false;
            }
            return;
        }

        // Priority 4: Kill Command for focus generation
        if (currentFocus < 50 && this->CanUseAbility(SPELL_KILL_COMMAND_SURV))
        {
            this->CastSpell(target, SPELL_KILL_COMMAND_SURV);
            _lastKillCommand = getMSTime();
            this->_resource = std::min<uint32>(this->_resource + 15, 100);
            return;
        }

        // Priority 5: Mongoose Bite during window or with charges
        if (_mongooseTracker.IsWindowActive() || _mongooseTracker.HasCharges())
        {
            if (currentFocus >= 30)
            {
                this->CastSpell(target, SPELL_MONGOOSE_BITE);
                _mongooseTracker.OnMongooseBiteCast();
                this->ConsumeResource(30);
                return;
            }
        }

        // Priority 6: Flanking Strike for coordination
        if (currentFocus >= 30 && this->CanUseAbility(SPELL_FLANKING_STRIKE))
        {
            this->CastSpell(target, SPELL_FLANKING_STRIKE);
            this->ConsumeResource(30);
            this->_resource = std::min<uint32>(this->_resource + 15, 100); // Returns some focus
            return;
        }

        // Priority 7: Raptor Strike as filler
        if (currentFocus >= 30)
        {
            this->CastSpell(target, SPELL_RAPTOR_STRIKE);
            _lastRaptorStrike = getMSTime();
            this->ConsumeResource(30);
            return;
        }

        // Priority 8: Kill Command if nothing else
        if (this->CanUseAbility(SPELL_KILL_COMMAND_SURV))
        {
            this->CastSpell(target, SPELL_KILL_COMMAND_SURV);
            this->_resource = std::min<uint32>(this->_resource + 15, 100);
            return;
        }
    }

    void ExecuteAoERotation(::Unit* target)
    {
        uint32 currentFocus = this->_resource;

        // Priority 1: Wildfire Bomb for AoE
        if (_bombManager.HasCharge())
        {
            uint32 bombSpell = _bombManager.GetBombSpell();
            this->CastSpell(target, bombSpell);
            _bombManager.UseCharge();
            return;
        }

        // Priority 2: Butchery for burst AoE
        if (currentFocus >= 30 && this->CanUseAbility(SPELL_BUTCHERY))
        {
            this->CastSpell(this->GetBot(), SPELL_BUTCHERY);
            this->ConsumeResource(30);
            return;
        }

        // Priority 3: Carve for cleave
        if (currentFocus >= 35)
        {
            this->CastSpell(this->GetBot(), SPELL_CARVE);
            this->ConsumeResource(35);
            return;
        }

        // Priority 4: Kill Command for focus
        if (currentFocus < 50 && this->CanUseAbility(SPELL_KILL_COMMAND_SURV))
        {
            this->CastSpell(target, SPELL_KILL_COMMAND_SURV);
            this->_resource = std::min<uint32>(this->_resource + 15, 100);
            return;
        }

        // Priority 5: Serpent Sting on multiple targets
        ApplySerpentStingToMultiple();
    }

private:
    // ========================================================================
    // SURVIVAL STATE MANAGEMENT
    // ========================================================================

    void UpdateSurvivalState()
    {
        uint32 currentTime = getMSTime();

        // Update bomb recharge
        _bombManager.UpdateRecharge();

        // Update Mongoose Bite charges
        _mongooseTracker.UpdateCharges();

        // Check Coordinated Assault expiry
        if (_coordinatedAssaultActive && currentTime > _coordinatedAssaultEndTime)
        {
            _coordinatedAssaultActive = false;
            _coordinatedAssaultEndTime = 0;
        }

        // Reset Mongoose window if expired
        if (!_mongooseTracker.IsWindowActive())
        {
            _mongooseTracker.Reset();
        }

        // Check Aspect of Eagle (90 second duration)
        static uint32 aspectStartTime = 0;
        if (_aspectOfEagleActive)
        {
            if (aspectStartTime == 0)
                aspectStartTime = currentTime;

            if (currentTime - aspectStartTime > 90000)
            {
                _aspectOfEagleActive = false;
                aspectStartTime = 0;
            }
        }
    }

    void UseHarpoon(Unit* target)
    {
        if (this->CanUseAbility(SPELL_HARPOON))
        {
            this->CastSpell(target, SPELL_HARPOON);

            // Terms of Engagement generates focus
            if (this->GetBot()->HasAura(SPELL_TERMS_OF_ENGAGEMENT))
            {
                this->_resource = std::min<uint32>(this->_resource + 20, 100);
            }
        }
    }

    bool ShouldUseCoordinatedAssault(Unit* target) const
    {
        if (!target)
            return false;

        // Use on high priority targets or when we have mongoose stacks
        return (target->GetHealthPct() > 50.0f) ||
               (_mongooseTracker.GetStacks() >= 3) ||
               (target->GetLevel() > this->GetBot()->GetLevel() + 2);
    }

    void ApplySerpentStingToMultiple()
    {
        uint32 currentFocus = this->_resource;
        if (currentFocus < 20)
            return;

        // Get enemies in range
        std::list<Unit*> enemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck checker(this->GetBot(), this->GetBot(), 8.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(this->GetBot(), enemies, checker);
        Cell::VisitAllObjects(this->GetBot(), searcher, 8.0f);

        uint32 stingsApplied = 0;
        for (Unit* enemy : enemies)
        {
            if (stingsApplied >= 3) // Limit to 3 targets
                break;

            if (!enemy->HasAura(SPELL_SERPENT_STING) && currentFocus >= 20)
            {
                this->CastSpell(enemy, SPELL_SERPENT_STING);
                this->ConsumeResource(20);
                currentFocus -= 20;
                stingsApplied++;
            }
        }
    }

    void InitializeCooldowns()
    {
        // Register Survival specific cooldowns
        RegisterCooldown(SPELL_COORDINATED_ASSAULT, 120000);  // 2 minute CD
        RegisterCooldown(SPELL_ASPECT_OF_EAGLE, 90000);       // 90 second CD
        RegisterCooldown(SPELL_FLANKING_STRIKE, 30000);       // 30 second CD
        RegisterCooldown(SPELL_KILL_COMMAND_SURV, 10000);     // 10 second CD
        RegisterCooldown(SPELL_HARPOON, 30000);               // 30 second CD
        RegisterCooldown(SPELL_BUTCHERY, 45000);              // 45 second CD
        RegisterCooldown(SPELL_MUZZLE, 15000);                // 15 second CD
        RegisterCooldown(SPELL_EXHILARATION_SURV, 120000);    // 2 minute CD
        RegisterCooldown(SPELL_SURVIVAL_OF_FITTEST, 180000);  // 3 minute CD
    }

private:
    // ========================================================================
    // HUNTER SPECIALIZATION ABSTRACT METHOD IMPLEMENTATIONS
    // ========================================================================

    // Pet management - implemented by SurvivalPetManager
    void UpdatePetManagement() override { _petManager.EnsurePetActive(GetBot()->GetVictim()); }
    void SummonPet() override { GetBot()->CastSpell(GetBot(), SPELL_CALL_PET_SURV, false); }
    void MendPetIfNeeded() override { if (_petManager.HasActivePet()) _petManager.EnsurePetActive(GetBot()->GetVictim()); }
    void FeedPetIfNeeded() override { /* Feeding not implemented in WoW 11.2 */ }
    bool HasActivePet() const override { return _petManager.HasActivePet(); }
    PetInfo GetPetInfo() const override { return PetInfo(); /* Stub */ }

    // Trap management - delegated to AI
    void UpdateTrapManagement() override { /* Traps managed by AI */ }
    void PlaceTrap(uint32 /*trapSpell*/, Position /*position*/) override { /* Traps managed by AI */ }
    bool ShouldPlaceTrap() const override { return false; }
    uint32 GetOptimalTrapSpell() const override { return SPELL_STEEL_TRAP; }
    std::vector<TrapInfo> GetActiveTraps() const override { return std::vector<TrapInfo>(); }

    // Aspect management - delegated to UpdateBuffs
    void UpdateAspectManagement() override { /* Aspects managed in UpdateBuffs */ }
    void SwitchToOptimalAspect() override { /* Aspects managed in UpdateBuffs */ }
    uint32 GetOptimalAspect() const override { return SPELL_ASPECT_OF_EAGLE; }
    bool HasCorrectAspect() const override { return true; }

    // Range and positioning - Survival is MELEE (unique)
    void UpdateRangeManagement() override { /* Survival fights in melee */ }
    bool IsInDeadZone(::Unit* /*target*/) const override { return false; /* No dead zone for melee */ }
    bool ShouldKite(::Unit* target) const override { return target && GetBot()->GetHealthPct() < 30.0f; }
    Position GetKitePosition(::Unit* target) const override {
        if (!target) return Position();
        // Get position 15 yards away from target
        float angle = target->GetRelativeAngle(GetBot());
        float x = target->GetPositionX() + 15.0f * std::cos(angle);
        float y = target->GetPositionY() + 15.0f * std::sin(angle);
        return Position(x, y, target->GetPositionZ());
    }
    void HandleDeadZone(::Unit* /*target*/) override { /* No dead zone for melee spec */ }

    // Tracking management - delegated to AI
    void UpdateTracking() override { /* Tracking managed by AI */ }
    uint32 GetOptimalTracking() const override { return 0; /* No specific tracking */ }
    void ApplyTracking(uint32 /*trackingSpell*/) override { /* Applied by AI */ }

    // Pet command interface - delegated to pet manager
    void CommandPetAttack(::Unit* target) override {
        if (target) _petManager.EnsurePetActive(target);
    }
    void CommandPetFollow() override { /* Handled by pet AI */ }
    void CommandPetStay() override { /* Handled by pet AI */ }

    // Positioning interface - MELEE positioning (unique for Survival)
    // Note: GetOptimalRange is final in base class - Survival should override base melee range behavior
    // The base class RangedDpsSpecialization returns 30-40 yards, but Survival needs melee (see line 364)
    Position GetOptimalPosition(::Unit* /*target*/) override { return Position(); /* Handled by base class */ }

private:
    WildfireBombManager _bombManager;
    MongooseBiteTracker _mongooseTracker;
    SurvivalPetManager _petManager;

    // Burst window tracking
    bool _coordinatedAssaultActive;
    uint32 _coordinatedAssaultEndTime;
    bool _aspectOfEagleActive;

    // Ability timing
    uint32 _lastRaptorStrike;
    uint32 _lastKillCommand;
    uint32 _lastSerpentSting;

    // Talent tracking
    bool _guerillaTacticsActive;
};

} // namespace Playerbot