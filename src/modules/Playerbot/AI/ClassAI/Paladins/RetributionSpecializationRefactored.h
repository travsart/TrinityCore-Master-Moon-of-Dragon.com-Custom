/*
 * Copyright (C) 2025 TrinityCore <https://www.trinitycore.org/>
 *
 * Retribution Paladin Specialization - REFACTORED
 *
 * This demonstrates how to migrate existing specializations to the new
 * template-based architecture, eliminating code duplication.
 *
 * BEFORE: 433 lines of code with duplicate UpdateCooldowns, CanUseAbility, etc.
 * AFTER: ~150 lines focusing ONLY on Retribution-specific logic
 */

#pragma once


#include "../Common/StatusEffectTracker.h"
#include "../Common/CooldownManager.h"
#include "../Common/RotationHelpers.h"
#include "../CombatSpecializationTemplates.h"
#include "../ResourceTypes.h"
#include "../CombatSpecializationTemplates.h"

// Phase 5 Integration: Decision Systems
#include "../../Decision/ActionPriorityQueue.h"
#include "../../Decision/BehaviorTree.h"
#include "../../BotAI.h"

namespace Playerbot
{


// Import BehaviorTree helper functions (avoid conflict with Playerbot::Action)
using bot::ai::Sequence;
using bot::ai::Selector;
using bot::ai::Condition;
using bot::ai::Inverter;
using bot::ai::Repeater;
using bot::ai::NodeStatus;

// Note: ::bot::ai::Action() conflicts with Playerbot::Action, use bot::ai::bot::ai::Action() explicitly
/**
 * Refactored Retribution Paladin using template architecture
 *
 * Key improvements:
 * - Inherits from MeleeDpsSpecialization<ManaResource> for role defaults
 * - Automatically gets UpdateCooldowns, CanUseAbility, OnCombatStart/End
 * - Uses HolyPowerSystem as secondary resource
 * - Eliminates ~280 lines of duplicate code
 */
class RetributionPaladinRefactored : public MeleeDpsSpecialization<ManaResource>, public PaladinSpecialization
{
public:
    using Base = MeleeDpsSpecialization<ManaResource>;
    using Base::GetBot;
    using Base::CastSpell;
    using Base::CanCastSpell;
    using Base::CanUseAbility;
    using Base::IsInMeleeRange;
    using Base::_resource;
    explicit RetributionPaladinRefactored(Player* bot)
        : MeleeDpsSpecialization<ManaResource>(bot)
        , PaladinSpecialization(bot)
        , _holyPower()
        , _hasArtOfWar(false)
        , _hasDivinePurpose(false)
        , _sealTwistWindow(0)
        , _cooldowns()
    {
        // Register cooldowns for major abilities
        // COMMENTED OUT:         _cooldowns.RegisterBatch({
        // COMMENTED OUT:             {RET_AVENGING_WRATH, 120000, 1},
        // COMMENTED OUT:             {RET_CRUSADE, 120000, 1},
        // COMMENTED OUT:             {RET_EXECUTION_SENTENCE, 60000, 1},
        // COMMENTED OUT:             {RET_FINAL_RECKONING, 60000, 1}
        // COMMENTED OUT:         });

        // Initialize Holy Power system
        _holyPower.Initialize(bot);

        // Initialize Phase 5 systems
        InitializeRetributionMechanics();
    }

    // ========================================================================
    // CORE ROTATION - Only Retribution-specific logic
    // ========================================================================

    void UpdateRotation(::Unit* target) override    {
        if (!target || !target->IsHostileTo(this->GetBot()))
            return;

        // Update procs and buffs
        CheckForProcs();

        // Execute priority rotation
        ExecutePriorityRotation(target);
    }

    void UpdateBuffs() override
    {
        Player* bot = this->GetBot();

        // Maintain Retribution Aura
        if (!bot->HasAura(SPELL_RETRIBUTION_AURA))
        {
            this->CastSpell(SPELL_RETRIBUTION_AURA, bot);
        }

        // Maintain Seal of Truth for single target
        if (!bot->HasAura(SPELL_SEAL_OF_TRUTH))
        {
            this->CastSpell(SPELL_SEAL_OF_TRUTH, bot);
        }

        // Blessings handled by group coordination
    }

protected:
    // ========================================================================
    // RETRIBUTION-SPECIFIC MECHANICS
    // ========================================================================

    /**
     * Execute abilities based on priority system
     */
    void ExecutePriorityRotation(::Unit* target)
    {
        // Hammer of Wrath (Execute phase)
        if (target->GetHealthPct() < 20.0f && this->CanUseAbility(SPELL_HAMMER_OF_WRATH))
        {
            this->CastSpell(SPELL_HAMMER_OF_WRATH, target);
            return;
        }

        // Templar's Verdict at 3+ Holy Power
        if (_holyPower.GetAvailable() >= 3 && this->CanUseAbility(SPELL_TEMPLARS_VERDICT))
        {
            this->CastSpell(SPELL_TEMPLARS_VERDICT, target);
            _holyPower.Consume(3);
            return;
        }

        // Divine Storm for AoE (3+ enemies)
        if (_holyPower.GetAvailable() >= 3 && this->GetEnemiesInRange(8.0f) >= 3)
        {
            if (this->CanUseAbility(SPELL_DIVINE_STORM))
            {
                this->CastSpell(SPELL_DIVINE_STORM, this->GetBot());
                _holyPower.Consume(3);
                return;
            }
        }

        // Crusader Strike - Primary Holy Power generator
        if (this->CanUseAbility(SPELL_CRUSADER_STRIKE))
        {
            this->CastSpell(SPELL_CRUSADER_STRIKE, target);
            _holyPower.Generate(1);
            return;
        }

        // Exorcism with Art of War proc
        if (_hasArtOfWar && this->CanUseAbility(SPELL_EXORCISM))
        {
            this->CastSpell(SPELL_EXORCISM, target);
            _hasArtOfWar = false;
            return;
        }

        // Judgment
        if (this->CanUseAbility(SPELL_JUDGMENT))
        {
            this->CastSpell(SPELL_JUDGMENT, target);
            return;
        }

        // Consecration if in melee range
        if (this->IsInMeleeRange(target) && this->CanUseAbility(SPELL_CONSECRATION))
        {
            this->CastSpell(SPELL_CONSECRATION, this->GetBot());
            return;
        }

        // Holy Wrath for burst
        if (ShouldUseCooldowns(target) && this->CanUseAbility(SPELL_HOLY_WRATH))
        {
            this->CastSpell(SPELL_HOLY_WRATH, target);
            return;
        }
    }

    /**
     * Check for Retribution-specific procs
     */
    void CheckForProcs()
    {
        Player* bot = this->GetBot();

        // Art of War proc (instant Exorcism)
        _hasArtOfWar = bot->HasAura(SPELL_ART_OF_WAR_PROC);

        // Divine Purpose proc (free 3 Holy Power ability)
        _hasDivinePurpose = bot->HasAura(SPELL_DIVINE_PURPOSE_PROC);
    }

    /**
     * Advanced seal twisting for extra DPS
     */
    void UpdateSealTwisting()
    {
        uint32 currentTime = GameTime::GetGameTimeMS();

        // Twist between Seal of Truth and Seal of Righteousness
        if (currentTime > _sealTwistWindow)
        {
            Player* bot = this->GetBot();

            if (bot->HasAura(SPELL_SEAL_OF_TRUTH))
            {
                // Quick swap to Righteousness for instant damage
                this->CastSpell(SPELL_SEAL_OF_RIGHTEOUSNESS, bot);
                _sealTwistWindow = currentTime + 100; // Very brief window
            }
            else
            {
                // Back to Truth for DoT
                this->CastSpell(SPELL_SEAL_OF_TRUTH, bot);
                _sealTwistWindow = currentTime + 10000; // 10 seconds
            }
        }
    }

    /**     * Determine if we should use offensive cooldowns
     */
    bool ShouldUseCooldowns(::Unit* target) const
    {
        // Use on bosses or when multiple enemies
        return (target->GetMaxHealth() > this->GetBot()->GetMaxHealth() * 10) ||               this->GetEnemiesInRange(10.0f) >= 3;
    }

    // ========================================================================
    // COMBAT LIFECYCLE HOOKS
    // ========================================================================

    void OnCombatStartSpecific(::Unit* target) override
    {
        // Pop offensive cooldowns at start for burst
        if (ShouldUseCooldowns(target))
        {
            Player* bot = this->GetBot();
            if (this->CanUseAbility(SPELL_AVENGING_WRATH))
            {
                this->CastSpell(SPELL_AVENGING_WRATH, bot);
            }

            if (this->CanUseAbility(SPELL_GUARDIAN_OF_ANCIENT_KINGS))
            {
                this->CastSpell(SPELL_GUARDIAN_OF_ANCIENT_KINGS, bot);
            }
        }

        // Reset Holy Power tracking
        _holyPower.Initialize(this->GetBot());
    }

    void OnCombatEndSpecific() override
    {
        // Reset proc tracking
        _hasArtOfWar = false;
        _hasDivinePurpose = false;
    }

    void InitializeRetributionMechanics()
    {
        using namespace bot::ai;
        using namespace BehaviorTreeBuilder;

        // ========================================================================
        // PHASE 5 INTEGRATION: ActionPriorityQueue (DPS + Holy Power Focus)
        // ========================================================================
        BotAI* ai = this;
        if (!ai)
            return;

        auto* queue = ai->GetActionPriorityQueue();
        if (queue)
        {
            // ====================================================================
            // CRITICAL TIER - Holy Power spenders (burst damage)
            // ====================================================================
            queue->RegisterSpell(SPELL_TEMPLARS_VERDICT,
                SpellPriority::CRITICAL,
                SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_TEMPLARS_VERDICT,
                [this](Player* bot, Unit*) {
                    // Use TV when we have 3+ HP
                    return this->_holyPower.GetAvailable() >= 3;
                },
                "3+ HP (burst single target)");

            queue->RegisterSpell(SPELL_DIVINE_STORM,
                SpellPriority::CRITICAL,
                SpellCategory::DAMAGE_AOE);
            queue->AddCondition(SPELL_DIVINE_STORM,
                [this](Player* bot, Unit*) {
                    // AoE spender when 3+ HP and 3+ enemies
                    return this->_holyPower.GetAvailable() >= 3 &&
                           bot->getAttackers().size() >= 3;
                },
                "3+ HP and 3+ enemies (AoE burst)");

            // ====================================================================
            // HIGH TIER - Holy Power generators and execute
            // ====================================================================
            queue->RegisterSpell(SPELL_CRUSADER_STRIKE,
                SpellPriority::HIGH,
                SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_CRUSADER_STRIKE,
                [this](Player* bot, Unit*) {
                    // Generate HP when below max
                    return this->_holyPower.GetAvailable() < 5;
                },
                "HP < 5 (HP generation)");

            queue->RegisterSpell(SPELL_HAMMER_OF_WRATH,
                SpellPriority::HIGH,
                SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_HAMMER_OF_WRATH,
                [](Player* bot, Unit* target) {
                    // Execute phase
                    return target && target->GetHealthPct() < 20.0f;
                },
                "Target < 20% (execute phase)");

            queue->RegisterSpell(SPELL_EXORCISM,
                SpellPriority::HIGH,
                SpellCategory::DAMAGE_SINGLE);
            queue->AddCondition(SPELL_EXORCISM,
                [this](Player* bot, Unit*) {
                    // Use with Art of War proc
                    return this->_hasArtOfWar;
                },
                "Art of War proc active");

            // ====================================================================
            // MEDIUM TIER - Standard rotation
            // ====================================================================
            queue->RegisterSpell(SPELL_JUDGMENT,
                SpellPriority::MEDIUM,
                SpellCategory::DAMAGE_SINGLE);

            queue->RegisterSpell(SPELL_CONSECRATION,
                SpellPriority::MEDIUM,
                SpellCategory::DAMAGE_AOE);
            queue->AddCondition(SPELL_CONSECRATION,
                [](Player* bot, Unit* target) {
                    // Use when in melee range
                    return target && bot->IsWithinMeleeRange(target);
                },
                "In melee range");

            queue->RegisterSpell(SPELL_AVENGING_WRATH,
                SpellPriority::MEDIUM,
                SpellCategory::OFFENSIVE);
            queue->AddCondition(SPELL_AVENGING_WRATH,
                [](Player* bot, Unit* target) {
                    // Use on bosses or packs
                    return (target && target->GetMaxHealth() > 500000) ||
                           bot->getAttackers().size() >= 3;
                },
                "Boss or 3+ enemies (burst)");

            queue->RegisterSpell(SPELL_GUARDIAN_OF_ANCIENT_KINGS,
                SpellPriority::MEDIUM,
                SpellCategory::OFFENSIVE);
            queue->AddCondition(SPELL_GUARDIAN_OF_ANCIENT_KINGS,
                [](Player* bot, Unit* target) {
                    // Offensive cooldown for burst
                    return (target && target->GetMaxHealth() > 500000) ||
                           bot->getAttackers().size() >= 3;
                },
                "Boss or 3+ enemies");

            // ====================================================================
            // LOW TIER - Fillers
            // ====================================================================
            queue->RegisterSpell(SPELL_HOLY_WRATH,
                SpellPriority::LOW,
                SpellCategory::DAMAGE_AOE);

            TC_LOG_INFO("module.playerbot", "  RETRIBUTION PALADIN: Registered {} spells in ActionPriorityQueue",
                queue->GetSpellCount());
        }

        // ========================================================================
        // PHASE 5 INTEGRATION: BehaviorTree (DPS + Holy Power Flow)
        // ========================================================================
        auto* behaviorTree = ai->GetBehaviorTree();
        if (behaviorTree)
        {
            auto root = Selector("Retribution Paladin DPS", {
                // ================================================================
                // TIER 1: EXECUTE PHASE (Target < 20%)
                // ================================================================
                Sequence("Execute Phase", {
                    Condition("Target < 20%", [](Player* bot, Unit* target) {
                        return target && target->GetHealthPct() < 20.0f;
                    }),
                    Selector("Execute Priority", {
                        // Hammer of Wrath (execute ability)
                        ::bot::ai::Action("Cast Hammer of Wrath", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_HAMMER_OF_WRATH, target))
                            {
                                this->CastSpell(SPELL_HAMMER_OF_WRATH, target);
                                this->_holyPower.Generate(1);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),
                        // Templar's Verdict if we have HP
                        ::bot::ai::Action("Cast Templar's Verdict", [this](Player* bot, Unit* target) {
                            if (this->_holyPower.GetAvailable() >= 3 &&
                                this->CanCastSpell(SPELL_TEMPLARS_VERDICT, target))
                            {
                                this->CastSpell(SPELL_TEMPLARS_VERDICT, target);
                                this->_holyPower.Consume(3);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                }),

                // ================================================================
                // TIER 2: HOLY POWER MANAGEMENT
                // ================================================================
                Sequence("Holy Power Management", {
                    Selector("HP Generation and Spending", {
                        // Spend HP at 3+
                        Sequence("Spend Holy Power", {
                            Condition("HP >= 3", [this](Player* bot, Unit*) {
                                return this->_holyPower.GetAvailable() >= 3;
                            }),
                            Selector("HP Spender Priority", {
                                // Divine Storm for AoE
                                Sequence("Divine Storm AoE", {
                                    Condition("3+ enemies", [](Player* bot, Unit*) {
                                        return bot->getAttackers().size() >= 3;
                                    }),
                                    ::bot::ai::Action("Cast Divine Storm", [this](Player* bot, Unit*) {
                                        if (this->CanCastSpell(SPELL_DIVINE_STORM, bot))
                                        {
                                            this->CastSpell(SPELL_DIVINE_STORM, bot);
                                            this->_holyPower.Consume(3);
                                            return NodeStatus::SUCCESS;
                                        }
                                        return NodeStatus::FAILURE;
                                    })
                                }),
                                // Templar's Verdict single target
                                ::bot::ai::Action("Cast Templar's Verdict", [this](Player* bot, Unit* target) {
                                    if (this->CanCastSpell(SPELL_TEMPLARS_VERDICT, target))
                                    {
                                        this->CastSpell(SPELL_TEMPLARS_VERDICT, target);
                                        this->_holyPower.Consume(3);
                                        return NodeStatus::SUCCESS;
                                    }
                                    return NodeStatus::FAILURE;
                                })
                            })
                        }),
                        // Generate HP
                        Sequence("Generate Holy Power", {
                            Condition("HP < 5", [this](Player* bot, Unit*) {
                                return this->_holyPower.GetAvailable() < 5;
                            }),
                            Selector("HP Generator Priority", {
                                // Crusader Strike
                                ::bot::ai::Action("Cast Crusader Strike", [this](Player* bot, Unit* target) {
                                    if (this->CanCastSpell(SPELL_CRUSADER_STRIKE, target))
                                    {
                                        this->CastSpell(SPELL_CRUSADER_STRIKE, target);
                                        this->_holyPower.Generate(1);
                                        return NodeStatus::SUCCESS;
                                    }
                                    return NodeStatus::FAILURE;
                                }),
                                // Exorcism with Art of War proc
                                Sequence("Exorcism on Proc", {
                                    Condition("Has Art of War proc", [this](Player* bot, Unit*) {
                                        return this->_hasArtOfWar;
                                    }),
                                    ::bot::ai::Action("Cast Exorcism", [this](Player* bot, Unit* target) {
                                        if (this->CanCastSpell(SPELL_EXORCISM, target))
                                        {
                                            this->CastSpell(SPELL_EXORCISM, target);
                                            this->_hasArtOfWar = false;
                                            return NodeStatus::SUCCESS;
                                        }
                                        return NodeStatus::FAILURE;
                                    })
                                })
                            })
                        })
                    })
                }),

                // ================================================================
                // TIER 3: COOLDOWN USAGE
                // ================================================================
                Sequence("Use Cooldowns", {
                    Condition("Boss or pack", [this](Player* bot, Unit* target) {
                        return this->ShouldUseCooldowns(target);
                    }),
                    Selector("Cooldown Priority", {
                        // Avenging Wrath
                        ::bot::ai::Action("Cast Avenging Wrath", [this](Player* bot, Unit*) {
                            if (this->CanCastSpell(SPELL_AVENGING_WRATH, bot))
                            {
                                this->CastSpell(SPELL_AVENGING_WRATH, bot);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),
                        // Guardian of Ancient Kings
                        ::bot::ai::Action("Cast Guardian", [this](Player* bot, Unit*) {
                            if (this->CanCastSpell(SPELL_GUARDIAN_OF_ANCIENT_KINGS, bot))
                            {
                                this->CastSpell(SPELL_GUARDIAN_OF_ANCIENT_KINGS, bot);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                }),

                // ================================================================
                // TIER 4: STANDARD DPS ROTATION
                // ================================================================
                Sequence("Standard Rotation", {
                    Selector("Rotation Priority", {
                        // Judgment
                        ::bot::ai::Action("Cast Judgment", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_JUDGMENT, target))
                            {
                                this->CastSpell(SPELL_JUDGMENT, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        }),

                        // Consecration (melee range)
                        Sequence("Consecration", {
                            Condition("In melee range", [](Player* bot, Unit* target) {
                                return target && bot->IsWithinMeleeRange(target);
                            }),
                            ::bot::ai::Action("Cast Consecration", [this](Player* bot, Unit*) {
                                if (this->CanCastSpell(SPELL_CONSECRATION, bot))
                                {
                                    this->CastSpell(SPELL_CONSECRATION, bot);
                                    return NodeStatus::SUCCESS;
                                }
                                return NodeStatus::FAILURE;
                            })
                        }),

                        // Holy Wrath filler
                        ::bot::ai::Action("Cast Holy Wrath", [this](Player* bot, Unit* target) {
                            if (this->CanCastSpell(SPELL_HOLY_WRATH, target))
                            {
                                this->CastSpell(SPELL_HOLY_WRATH, target);
                                return NodeStatus::SUCCESS;
                            }
                            return NodeStatus::FAILURE;
                        })
                    })
                })
            });

            behaviorTree->SetRoot(root);
            TC_LOG_INFO("module.playerbot", " RETRIBUTION PALADIN: BehaviorTree initialized with DPS flow");
        }
    }

private:
    CooldownManager _cooldowns;
    // ========================================================================
    // SPELL IDS
    // ========================================================================

    enum RetributionSpells
    {
        // Seals
        SPELL_SEAL_OF_TRUTH         = 31801,
        SPELL_SEAL_OF_RIGHTEOUSNESS = 21084,

        // Auras
        SPELL_RETRIBUTION_AURA      = 7294,

        // Core Abilities
        SPELL_CRUSADER_STRIKE       = 35395,
        SPELL_TEMPLARS_VERDICT      = 85256,
        SPELL_DIVINE_STORM          = 53385,
        SPELL_HAMMER_OF_WRATH       = 24275,
        SPELL_EXORCISM              = 879,
        SPELL_JUDGMENT              = 20271,
        SPELL_CONSECRATION          = 26573,
        SPELL_HOLY_WRATH            = 2812,

        // Cooldowns
        SPELL_AVENGING_WRATH        = 31884,
        SPELL_GUARDIAN_OF_ANCIENT_KINGS = 86150,

        // Procs
        SPELL_ART_OF_WAR_PROC       = 59578,
        SPELL_DIVINE_PURPOSE_PROC   = 90174,
    };

    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    // Secondary resource system
    HolyPowerSystem _holyPower;

    // Proc tracking
    bool _hasArtOfWar;
    bool _hasDivinePurpose;

    // Seal twisting
    uint32 _sealTwistWindow;
};

} // namespace Playerbot
