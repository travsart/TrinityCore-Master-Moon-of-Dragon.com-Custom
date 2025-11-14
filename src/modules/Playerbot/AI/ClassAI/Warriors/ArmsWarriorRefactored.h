/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Arms Warrior Specialization - REFACTORED
 *
 * This demonstrates the complete migration of Arms Warrior to the template-based
 * architecture, eliminating code duplication while maintaining full functionality.
 *
 * BEFORE: 620 lines of code with duplicate UpdateCooldowns, CanUseAbility, etc.
 * AFTER: ~250 lines focusing ONLY on Arms-specific logic
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "WarriorAI.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
// Phase 5 Integration
#include "../../Decision/ActionPriorityQueue.h"
#include "../../Decision/BehaviorTree.h"
#include "../../BotAI.h"
#include <unordered_map>
#include <queue>

namespace Playerbot
{

// Import BehaviorTree helper functions (avoid conflict with Playerbot::Action)
using bot::ai::Sequence;
using bot::ai::Selector;
using bot::ai::Condition;
using bot::ai::Inverter;
using bot::ai::Repeater;
using bot::ai::NodeStatus;

// Note: bot::ai::Action() conflicts with Playerbot::Action, use bot::ai::bot::ai::Action() explicitly

/**
 * Refactored Arms Warrior using template architecture
 *
 * Key improvements:
 * - Inherits from MeleeDpsSpecialization<RageResource> for role defaults
 * - Automatically gets UpdateCooldowns, CanUseAbility, OnCombatStart/End
 * - Uses specialized rage management as primary resource
 * - Eliminates ~370 lines of duplicate code
 */
class ArmsWarriorRefactored : public MeleeDpsSpecialization<RageResource>
{
private:
    // ========================================================================
    // SPELL IDS
    // ========================================================================

    enum ArmsSpells
    {
        // Stances
        SPELL_BATTLE_STANCE         = 2457,
        SPELL_DEFENSIVE_STANCE      = 71,
        SPELL_BERSERKER_STANCE      = 2458,

        // Shouts
        SPELL_BATTLE_SHOUT          = 6673,
        SPELL_COMMANDING_SHOUT      = 469,

        // Core Abilities
        SPELL_MORTAL_STRIKE         = 12294,
        SPELL_COLOSSUS_SMASH        = 86346,
        SPELL_OVERPOWER             = 7384,
        SPELL_EXECUTE               = 5308,
        SPELL_WHIRLWIND             = 1680,
        SPELL_REND                  = 772,
        SPELL_HEROIC_STRIKE         = 78,
        SPELL_CLEAVE                = 845,
        SPELL_CHARGE                = 100,

        // Arms Specific
        SPELL_WAR_BREAKER           = 262161,
        SPELL_SWEEPING_STRIKES      = 260708,
        SPELL_BLADESTORM            = 227847,
        SPELL_AVATAR                = 107574,
        SPELL_DEEP_WOUNDS           = 115767,
        SPELL_TACTICAL_MASTERY      = 12295,

        // Procs
        SPELL_OVERPOWER_PROC        = 60503,
        SPELL_SUDDEN_DEATH_PROC     = 52437,
    };

public:
    // Use base class members with type alias for cleaner syntax
    using Base = MeleeDpsSpecialization<RageResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::_resource;
    using Base::IsInMeleeRange;
    using Base::CanUseAbility;

    // BehaviorTree helper functions (must be accessible in member functions)
    using bot::ai::Sequence;
    using bot::ai::Selector;
    using bot::ai::Condition;
    using bot::ai::Inverter;
    using bot::ai::Repeater;
    using bot::ai::NodeStatus;

private:
    // Forward declarations for methods called in constructor
    void InitializeDebuffTracking();
    void InitializeArmsRotation();
    bool IsExecutePhase(::Unit* target) const;
    bool ShouldUseColossusSmash(::Unit* target) const;
    bool ShouldUseBladestorm() const;
    bool ShouldUseAvatar(::Unit* target) const;
    bool HasRendDebuff(::Unit* target) const;
    void CleanupExpiredDeepWounds();

public:
    explicit ArmsWarriorRefactored(Player* bot)
        : MeleeDpsSpecialization<RageResource>(bot)
        , _deepWoundsTracking()
        , _colossusSmashActive(false)
        , _overpowerReady(false)
        , _suddenDeathProc(false)
        , _executePhaseActive(false)
        , _lastMortalStrike(0)
        , _lastColossusSmash(0)
        , _tacticalMasteryRage(0)
        , _currentStance(WarriorAI::WarriorStance::BATTLE)
        , _preferredStance(WarriorAI::WarriorStance::BATTLE)
    {
        // Initialize debuff tracking
        InitializeDebuffTracking();

        // Setup Arms-specific mechanics
        InitializeArmsRotation();
    }

    // ========================================================================
    // CORE ROTATION - Only Arms-specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsAlive() || !target->IsHostileTo(this->GetBot()))
            return;

        // Update Arms-specific mechanics
        UpdateArmsState(target);

        // Execute phase has highest priority
        if (IsExecutePhase(target))
        {
            ExecutePhaseRotation(target);
            return;
        }

        // Standard rotation
        ExecuteArmsRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();

        // Maintain Battle Shout
        if (!bot->HasAura(SPELL_BATTLE_SHOUT) && !bot->HasAura(SPELL_COMMANDING_SHOUT))
        {
            this->CastSpell(SPELL_BATTLE_SHOUT, bot);
        }

        // Sweeping Strikes for multiple enemies
        if (this->GetEnemiesInRange(8.0f) >= 2 && !bot->HasAura(SPELL_SWEEPING_STRIKES))
        {
            if (this->CanUseAbility(SPELL_SWEEPING_STRIKES))
            {
                this->CastSpell(SPELL_SWEEPING_STRIKES, bot);
            }
        }

        // Stance management
        UpdateStanceOptimization();
    }

protected:
    // ========================================================================
    // RESOURCE MANAGEMENT OVERRIDE
    // ========================================================================

    uint32 GetSpellResourceCost(uint32 spellId) const override
    {
        switch (spellId)
        {
            case SPELL_MORTAL_STRIKE:    return 30;
            case SPELL_COLOSSUS_SMASH:   return 20;
            case SPELL_OVERPOWER:        return 5;
            case SPELL_EXECUTE:          return 15; // Base cost, scales with available rage
            case SPELL_WHIRLWIND:        return 25;
            case SPELL_REND:             return 10;
            case SPELL_HEROIC_STRIKE:    return 15;
            case SPELL_CLEAVE:           return 20;
            default:                     return 10;
        }
    }

    // ========================================================================
    // ARMS-SPECIFIC ROTATION LOGIC
    // ========================================================================

    void ExecuteArmsRotation(::Unit* target)
    {
        // Priority 1: Colossus Smash for vulnerability window
        if (ShouldUseColossusSmash(target) && this->CanUseAbility(SPELL_COLOSSUS_SMASH))
        {
            this->CastSpell(SPELL_COLOSSUS_SMASH, target);
            _colossusSmashActive = true;
            _lastColossusSmash = GameTime::GetGameTimeMS();
            return;
        }

        // Priority 2: Bladestorm for burst AoE
        if (ShouldUseBladestorm() && this->CanUseAbility(SPELL_BLADESTORM))
        {
            this->CastSpell(SPELL_BLADESTORM, this->GetBot());
            return;
        }

        // Priority 3: Avatar for damage increase
        if (ShouldUseAvatar(target) && this->CanUseAbility(SPELL_AVATAR))
        {
            this->CastSpell(SPELL_AVATAR, this->GetBot());
            return;
        }

        // Priority 4: Mortal Strike - Primary damage and healing reduction
        if (this->CanUseAbility(SPELL_MORTAL_STRIKE))
        {
            this->CastSpell(SPELL_MORTAL_STRIKE, target);
            _lastMortalStrike = GameTime::GetGameTimeMS();
            ApplyDeepWounds(target);
            return;
        }

        // Priority 5: Overpower when proc is available
        if (_overpowerReady && this->CanUseAbility(SPELL_OVERPOWER))
        {
            this->CastSpell(SPELL_OVERPOWER, target);
            _overpowerReady = false;
            ApplyDeepWounds(target);
            return;
        }

        // Priority 6: War Breaker for AoE debuff
        if (this->GetEnemiesInRange(8.0f) >= 2 && this->CanUseAbility(SPELL_WAR_BREAKER))
        {
            this->CastSpell(SPELL_WAR_BREAKER, target);
            return;
        }

        // Priority 7: Whirlwind for AoE
        if (this->GetEnemiesInRange(8.0f) >= 2 && this->CanUseAbility(SPELL_WHIRLWIND))
        {
            this->CastSpell(SPELL_WHIRLWIND, this->GetBot());
            return;
        }        // Priority 8: Rend for DoT (if not already applied)
        if (!HasRendDebuff(target) && this->_resource >= 10 && this->CanUseAbility(SPELL_REND))
        {
            this->CastSpell(SPELL_REND, target);
            _rendTracking[target->GetGUID()] = GameTime::GetGameTimeMS() + 21000;            return;
        }

        // Priority 9: Heroic Strike as rage dump
        if (this->_resource >= 80 && this->CanUseAbility(SPELL_HEROIC_STRIKE))
        {
            this->CastSpell(SPELL_HEROIC_STRIKE, target);
            return;
        }
    }

    void ExecutePhaseRotation(::Unit* target)
    {
        // Switch to Berserker Stance for execute if needed
        if (_currentStance != WarriorAI::WarriorStance::BERSERKER && this->HasTacticalMastery())
        {
            this->SwitchToStance(WarriorAI::WarriorStance::BERSERKER);
        }

        // Priority 1: Execute with Sudden Death proc
        if (_suddenDeathProc && this->CanUseAbility(SPELL_EXECUTE))
        {
            this->CastSpell(SPELL_EXECUTE, target);
            _suddenDeathProc = false;
            return;
        }

        // Priority 2: Colossus Smash for execute damage
        if (!_colossusSmashActive && this->CanUseAbility(SPELL_COLOSSUS_SMASH))
        {
            this->CastSpell(SPELL_COLOSSUS_SMASH, target);
            _colossusSmashActive = true;
            _lastColossusSmash = GameTime::GetGameTimeMS();
            return;
        }

        // Priority 3: Execute spam with available rage
        if (this->CanUseAbility(SPELL_EXECUTE))
        {
            // Execute consumes up to 40 additional rage for bonus damage
            if (this->_resource >= 15) // Base cost
            {
                this->CastSpell(SPELL_EXECUTE, target);
                return;
            }
        }

        // Priority 4: Mortal Strike to maintain pressure
        if (this->CanUseAbility(SPELL_MORTAL_STRIKE))
        {
            this->CastSpell(SPELL_MORTAL_STRIKE, target);
            _lastMortalStrike = GameTime::GetGameTimeMS();
            return;
        }

        // Priority 5: Overpower if available
        if (_overpowerReady && this->CanUseAbility(SPELL_OVERPOWER))
        {
            this->CastSpell(SPELL_OVERPOWER, target);
            _overpowerReady = false;
            return;
        }
    }

    // ========================================================================
    // ARMS-SPECIFIC STATE MANAGEMENT
    // ========================================================================

    void UpdateArmsState(::Unit* target)
    {
        Player* bot = this->GetBot();
        uint32 currentTime = GameTime::GetGameTimeMS();

        // Check for Overpower proc (after dodge/parry)
        _overpowerReady = bot->HasAura(SPELL_OVERPOWER_PROC);

        // Check for Sudden Death proc (free Execute)
        _suddenDeathProc = bot->HasAura(SPELL_SUDDEN_DEATH_PROC);

        // Update Colossus Smash tracking
        if (_colossusSmashActive && currentTime > _lastColossusSmash + 10000)
        {
            _colossusSmashActive = false;
        }

        // Update Deep Wounds tracking
        CleanupExpiredDeepWounds();

        // Update execute phase state
        _executePhaseActive = (target->GetHealthPct() <= 20.0f);
    }

    void UpdateStanceOptimization()
    {
        Player* bot = this->GetBot();

        // Determine optimal stance based on situation
        WarriorAI::WarriorStance optimalStance = this->DetermineOptimalStance();

        if (_currentStance != optimalStance)
        {
            // Tactical Mastery allows retaining rage when switching
            if (this->HasTacticalMastery())
            {
                _tacticalMasteryRage = ::std::min(this->_resource, 25u);
            }

            this->SwitchToStance(optimalStance);
        }
    }

    WarriorAI::WarriorStance DetermineOptimalStance()
    {
        Player* bot = this->GetBot();

        // Defensive stance if low health
        if (bot->GetHealthPct() < 30.0f)
            return WarriorAI::WarriorStance::DEFENSIVE;

        // Berserker for execute phase
        if (_executePhaseActive)
            return WarriorAI::WarriorStance::BERSERKER;

        // Battle stance as default for Arms
        return WarriorAI::WarriorStance::BATTLE;
    }

    void SwitchToStance(WarriorAI::WarriorStance stance)
    {
        uint32 stanceSpell = 0;
        switch (stance)
        {
            case WarriorAI::WarriorStance::BATTLE:     stanceSpell = SPELL_BATTLE_STANCE; break;
            case WarriorAI::WarriorStance::DEFENSIVE:  stanceSpell = SPELL_DEFENSIVE_STANCE; break;
            case WarriorAI::WarriorStance::BERSERKER:  stanceSpell = SPELL_BERSERKER_STANCE; break;
            default: return;
        }

        if (stanceSpell && this->CanUseAbility(stanceSpell))
        {
            this->CastSpell(stanceSpell, this->GetBot());
            _currentStance = stance;
        }
    }    // ========================================================================
    // DEEP WOUNDS MANAGEMENT
    // ========================================================================

    void ApplyDeepWounds(::Unit* target)
    {
        if (!target)
            return;

        // Deep Wounds is applied by critical strikes
        _deepWoundsTracking[target->GetGUID()] = GameTime::GetGameTimeMS() + 21000; // 21 second duration
    }

    void CleanupExpiredDeepWounds()
    {
        uint32 currentTime = GameTime::GetGameTimeMS();
        for (auto it = _deepWoundsTracking.begin(); it != _deepWoundsTracking.end();)
        {
            if (it->second < currentTime)
                it = _deepWoundsTracking.erase(it);
            else
                ++it;
        }
    }

    // ========================================================================
    // CONDITION CHECKS
    // ========================================================================

    bool IsExecutePhase(::Unit* target) const
    {
        return target && (target->GetHealthPct() <= 20.0f || _suddenDeathProc);
    }

    bool ShouldUseColossusSmash(::Unit* target) const    {
        // Use on cooldown for damage window
        return !_colossusSmashActive && target;
    }

    bool ShouldUseBladestorm() const
    {
        // Use for 3+ enemies or burst phase
        return this->GetEnemiesInRange(8.0f) >= 3 || this->_resource >= 80;
    }

    bool ShouldUseAvatar(::Unit* target) const
    {
        // Use during Colossus Smash window or execute phase
        return target && (_colossusSmashActive || _executePhaseActive);
    }

    bool HasRendDebuff(::Unit* target) const
    {
        if (!target)
            return false;

        auto it = _rendTracking.find(target->GetGUID());        return it != _rendTracking.end() && it->second > GameTime::GetGameTimeMS();
    }

    bool HasTacticalMastery() const
    {
        return this->GetBot()->HasSpell(SPELL_TACTICAL_MASTERY);
    }

    // ========================================================================
    // COMBAT LIFECYCLE HOOKS
    // ========================================================================

    void OnCombatStartSpecific(::Unit* target) override
    {
        // Reset all Arms-specific state
        _colossusSmashActive = false;
        _overpowerReady = false;
        _suddenDeathProc = false;
        _executePhaseActive = false;
        _lastMortalStrike = 0;
        _lastColossusSmash = 0;
        _deepWoundsTracking.clear();
        _rendTracking.clear();

        // Start in Battle Stance
        if (_currentStance != WarriorAI::WarriorStance::BATTLE)
        {
            this->SwitchToStance(WarriorAI::WarriorStance::BATTLE);
        }

        // Use charge if not in range
        if (!this->IsInMeleeRange(target) && this->CanUseAbility(SPELL_CHARGE))
        {
            this->CastSpell(SPELL_CHARGE, target);
        }
    }

    void OnCombatEndSpecific() override
    {
        // Clear all combat state
        _colossusSmashActive = false;
        _overpowerReady = false;
        _suddenDeathProc = false;
        _executePhaseActive = false;
        _deepWoundsTracking.clear();
        _rendTracking.clear();
    }

private:
    CooldownManager _cooldowns;
    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    void InitializeDebuffTracking()
    {
        _deepWoundsTracking.clear();
        _rendTracking.clear();
    }

    void InitializeArmsRotation()
    {
        using namespace bot::ai;
        using namespace BehaviorTreeBuilder;

        // Setup any Arms-specific initialization
        _tacticalMasteryRage = 0;

        // ========================================================================
        // PHASE 5 INTEGRATION: ActionPriorityQueue
        // ========================================================================
        BotAI* ai = this;
        if (!ai)
            return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // Emergency spells
            queue->RegisterSpell(SPELL_EXECUTE, SpellPriority::EMERGENCY, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_EXECUTE,
                ::std::function<bool(Player*, Unit*)>{[](Player* bot, Unit* target) {
                    return target && target->GetHealthPct() < 20.0f;
                }},
                "Target HP < 20% (Execute range)");

            // Critical cooldowns
            queue->RegisterSpell(SPELL_COLOSSUS_SMASH, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);
            queue->RegisterSpell(SPELL_BLADESTORM, SpellPriority::CRITICAL, SpellCategory::DAMAGE_AOE);
            queue->RegisterSpell(SPELL_AVATAR, SpellPriority::CRITICAL, SpellCategory::OFFENSIVE);

            // High priority core rotation
            queue->RegisterSpell(SPELL_MORTAL_STRIKE, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->RegisterSpell(SPELL_OVERPOWER, SpellPriority::HIGH, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_OVERPOWER,
                [](Player* bot, Unit*) {
                    return bot->HasAura(SPELL_OVERPOWER_PROC);
                },
                "Overpower proc active");

            // Medium priority
            queue->RegisterSpell(SPELL_WHIRLWIND, SpellPriority::MEDIUM, SpellCategory::DAMAGE_AOE);
            queue->AddCondition(SPELL_WHIRLWIND,
                ::std::function<bool(Player*, Unit*)>{[this](Player* bot, Unit* target) {
                    // Capture 'this' for member access if needed
                    return bot->getAttackers().size() >= 3;
                }},
                "3+ targets (AoE)");

            queue->RegisterSpell(SPELL_REND, SpellPriority::MEDIUM, SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_REND,
                ::std::function<bool(Player*, Unit*)>{[](Player* bot, Unit* target) {
                    return target && !target->HasAura(SPELL_REND);
                }},
                "Rend not active on target");

            // Low priority fillers
            queue->RegisterSpell(SPELL_HEROIC_STRIKE, SpellPriority::LOW, SpellCategory::DAMAGE_SINGLE);
            queue->RegisterSpell(SPELL_CLEAVE, SpellPriority::LOW, SpellCategory::DAMAGE_AOE);

            TC_LOG_INFO("module.playerbot", "  ARMS WARRIOR: Registered {} spells in ActionPriorityQueue",
                queue->GetSpellCount());
        }

        // ========================================================================
        // PHASE 5 INTEGRATION: BehaviorTree
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Arms Warrior Combat", {
                // ============================================================
                // 1. EXECUTE PHASE (Target < 20% HP)
                // ============================================================
                Sequence("Execute Phase", {
                    Condition("Target < 20% HP", [](Player* bot, Unit* target) {
                        return target && target->GetHealthPct() < 20.0f;
                    }),
                    Selector("Execute Priority", {
                        bot::ai::Action("Cast Execute", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_EXECUTE, target))
                            {
                                this->CastSpell(SPELL_EXECUTE, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),
                        bot::ai::Action("Cast Mortal Strike (Execute Phase)", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_MORTAL_STRIKE, target))
                            {
                                this->CastSpell(SPELL_MORTAL_STRIKE, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                }),

                // ============================================================
                // 2. COOLDOWN USAGE (Boss fights, burst windows)
                // ============================================================
                Sequence("Use Major Cooldowns", {
                    Condition("Should use cooldowns", [](Player* bot, Unit* target) {
                        // Use cooldowns on bosses or high HP targets
                        return target && (target->GetCreatureType() == CREATURE_TYPE_HUMANOID ||
                                        target->GetMaxHealth() > 500000);
                    }),
                    Selector("Cooldown Priority", {
                        bot::ai::Action("Cast Avatar", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_AVATAR, bot))
                            {
                                this->CastSpell(SPELL_AVATAR, bot);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),
                        bot::ai::Action("Cast Bladestorm", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_BLADESTORM, bot))
                            {
                                this->CastSpell(SPELL_BLADESTORM, bot);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                }),

                // ============================================================
                // 3. STANDARD ROTATION
                // ============================================================
                Sequence("Standard Rotation", {
                    // Maintain Colossus Smash debuff
                    Selector("Maintain Colossus Smash", {
                        Condition("CS Active", [](Player* bot, Unit* target) {
                            return target && target->HasAura(SPELL_COLOSSUS_SMASH);
                        }),
                        bot::ai::Action("Cast Colossus Smash", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_COLOSSUS_SMASH, target))
                            {
                                this->CastSpell(SPELL_COLOSSUS_SMASH, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    }),

                    // Cast Mortal Strike on cooldown
                    Selector("Mortal Strike", {
                        bot::ai::Action("Cast Mortal Strike", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_MORTAL_STRIKE, target))
                            {
                                this->CastSpell(SPELL_MORTAL_STRIKE, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    }),

                    // Cast Overpower on proc
                    Sequence("Overpower on Proc", {
                        Condition("Has Overpower Proc", [](Player* bot, Unit*) {
                            return bot->HasAura(SPELL_OVERPOWER_PROC);
                        }),
                        bot::ai::Action("Cast Overpower", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_OVERPOWER, target))
                            {
                                this->CastSpell(SPELL_OVERPOWER, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    }),

                    // Filler spells
                    Selector("Filler", {
                        bot::ai::Action("Cast Whirlwind (AoE)", [this](Player* bot, Unit* target) {
                            if (bot->getAttackers().size() >= 3 && this->CanCastSpell(SPELL_WHIRLWIND, target))
                            {
                                this->CastSpell(SPELL_WHIRLWIND, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),
                        bot::ai::Action("Cast Heroic Strike", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_HEROIC_STRIKE, target))
                            {
                                this->CastSpell(SPELL_HEROIC_STRIKE, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                })
            });

            behaviorTree->SetRoot(root);
            TC_LOG_INFO("module.playerbot", " ARMS WARRIOR: BehaviorTree initialized with hierarchical combat flow");
        }
    }

    // Note: WarriorStance enum is inherited from WarriorSpecialization parent class

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // Debuff tracking
    ::std::unordered_map<ObjectGuid, uint32> _deepWoundsTracking;
    ::std::unordered_map<ObjectGuid, uint32> _rendTracking;

    // State tracking
    bool _colossusSmashActive;
    bool _overpowerReady;
    bool _suddenDeathProc;
    bool _executePhaseActive;

    // Timing tracking
    uint32 _lastMortalStrike;
    uint32 _lastColossusSmash;

    // Stance management
    uint32 _tacticalMasteryRage;
    WarriorAI::WarriorStance _currentStance;
    WarriorAI::WarriorStance _preferredStance;

};

} // namespace Playerbot
