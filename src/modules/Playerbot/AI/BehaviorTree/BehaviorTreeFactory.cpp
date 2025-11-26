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

#include "BehaviorTreeFactory.h"
#include "Log.h"
#include "ObjectAccessor.h"

namespace Playerbot
{

// Static member initialization
::std::unordered_map<::std::string, ::std::function<::std::shared_ptr<BTNode>()>> BehaviorTreeFactory::_customTreeBuilders;

::std::shared_ptr<BTNode> BehaviorTreeFactory::CreateTree(TreeType type)
{
    switch (type)
    {
        case TreeType::MELEE_COMBAT:
            return BuildMeleeCombatTree();
        case TreeType::RANGED_COMBAT:
            return BuildRangedCombatTree();
        case TreeType::TANK_COMBAT:
            return BuildTankCombatTree();
        case TreeType::SINGLE_TARGET_HEALING:
            return BuildSingleTargetHealingTree();
        case TreeType::GROUP_HEALING:
            return BuildGroupHealingTree();
        case TreeType::DISPEL_PRIORITY:
            return BuildDispelPriorityTree();
        case TreeType::FOLLOW_LEADER:
            return BuildFollowLeaderTree();
        case TreeType::COMBAT_POSITIONING:
            return BuildCombatPositioningTree();
        case TreeType::FLEE_TO_SAFETY:
            return BuildFleeToSafetyTree();
        case TreeType::BUFF_MAINTENANCE:
            return BuildBuffMaintenanceTree();
        case TreeType::RESOURCE_MANAGEMENT:
            return BuildResourceManagementTree();
        default:
            TC_LOG_ERROR("playerbot.bt", "Unknown tree type requested: {}", uint32(type));
            return nullptr;
    }
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildMeleeCombatTree()
{
    // Root: Selector (try combat or flee)
    auto root = ::std::make_shared<BTSelector>("MeleeCombatRoot");

    // Branch 1: Flee if critically wounded
    auto fleeCondition = ::std::make_shared<BTCondition>("CriticalHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;
            return bot->GetHealthPct() < 20.0f && bot->IsInCombat();
        }
    );
    root->AddChild(fleeCondition);

    // Branch 2: Melee combat sequence
    auto combatSequence = ::std::make_shared<BTSequence>("MeleeCombat");

    // Check has target
    combatSequence->AddChild(::std::make_shared<BTCheckHasTarget>());

    // Check in melee range (0-5 yards)
    combatSequence->AddChild(::std::make_shared<BTCheckInRange>(0.0f, 5.0f));

    // Use defensive cooldowns if low health
    auto defensiveSelector = ::std::make_shared<BTSelector>("DefensiveCooldowns");
    defensiveSelector->AddChild(::std::make_shared<BTCondition>("HighHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            return bot && bot->GetHealthPct() > 50.0f;
        }
    ));
    // If low health, try to use defensive CD (placeholder - class-specific)
    defensiveSelector->AddChild(::std::make_shared<BTAction>("UseDefensive",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific defensive cooldowns implementation required
            // When implementing per-class defensive behaviors, add logic here for:
            // - Warriors: Shield Wall, Last Stand, Shield Block
            // - Paladins: Divine Shield, Divine Protection, Lay on Hands
            // - Death Knights: Icebound Fortitude, Anti-Magic Shell, Vampiric Blood
            // - Druids: Barkskin, Survival Instincts, Frenzied Regeneration
            // - Monks: Fortifying Brew, Dampen Harm, Diffuse Magic
            return BTStatus::SUCCESS;
        }
    ));
    combatSequence->AddChild(defensiveSelector);

    // Face target
    combatSequence->AddChild(::std::make_shared<BTFaceTarget>());

    // Execute melee attack
    combatSequence->AddChild(::std::make_shared<BTMeleeAttack>());

    root->AddChild(combatSequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildRangedCombatTree()
{
    auto root = ::std::make_shared<BTSelector>("RangedCombatRoot");

    // Flee if critically wounded
    auto fleeSequence = ::std::make_shared<BTSequence>("FleeIfCritical");
    fleeSequence->AddChild(::std::make_shared<BTCondition>("CriticalHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            return bot && bot->GetHealthPct() < 20.0f && bot->IsInCombat();
        }
    ));
    fleeSequence->AddChild(::std::make_shared<BTAction>("StartFleeing",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            bb.Set<bool>("ShouldFlee", true);
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(fleeSequence);

    // Ranged combat sequence
    auto combatSequence = ::std::make_shared<BTSequence>("RangedCombat");

    combatSequence->AddChild(::std::make_shared<BTCheckHasTarget>());

    // Check in ranged range (5-40 yards)
    auto rangeCheck = ::std::make_shared<BTSelector>("RangeManagement");
    rangeCheck->AddChild(::std::make_shared<BTCheckInRange>(5.0f, 40.0f));

    // If not in range, move to optimal range
    auto moveSequence = ::std::make_shared<BTSequence>("MoveToRange");
    moveSequence->AddChild(::std::make_shared<BTCondition>("OutOfRange",
        [](BotAI* ai, BTBlackboard& bb) {
            Unit* target = nullptr;
            if (!bb.Get<Unit*>("CurrentTarget", target) || !target)
                return false;

            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            float distance = bot->GetDistance(target);
            return distance < 5.0f || distance > 40.0f;
        }
    ));
    moveSequence->AddChild(::std::make_shared<BTMoveToTarget>(20.0f, 35.0f));
    rangeCheck->AddChild(moveSequence);

    combatSequence->AddChild(rangeCheck);

    // Face target
    combatSequence->AddChild(::std::make_shared<BTFaceTarget>());

    // Cast ranged abilities (placeholder - class-specific)
    combatSequence->AddChild(::std::make_shared<BTAction>("CastRangedSpell",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific ranged spell rotation implementation required
            // When implementing per-class ranged combat, add logic here for:
            // - Hunters: Steady Shot, Arcane Shot, Kill Command, Multi-Shot
            // - Mages: Fireball, Frostbolt, Arcane Blast, AoE spells
            // - Warlocks: Shadow Bolt, Incinerate, Corruption, Immolate
            // - Priests (Shadow): Mind Blast, Mind Flay, Shadow Word: Pain
            // - Balance Druids: Wrath, Starfire, Moonfire, Sunfire
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(combatSequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildTankCombatTree()
{
    auto root = ::std::make_shared<BTSelector>("TankCombatRoot");

    // Use defensive CDs if critical
    auto emergencyDefensive = ::std::make_shared<BTSequence>("EmergencyDefensive");
    emergencyDefensive->AddChild(::std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    emergencyDefensive->AddChild(::std::make_shared<BTCheckInCombat>());
    emergencyDefensive->AddChild(::std::make_shared<BTAction>("UseEmergencyCD",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific emergency defensive cooldowns implementation required
            // When implementing per-class tank survival abilities, add logic here for:
            // - Warriors: Shield Wall, Last Stand, Rallying Cry
            // - Paladins: Ardent Defender, Guardian of Ancient Kings, Divine Shield
            // - Death Knights: Dancing Rune Weapon, Vampiric Blood, Icebound Fortitude
            // - Druids: Survival Instincts, Barkskin, Frenzied Regeneration
            // - Monks: Fortifying Brew, Zen Meditation, Dampen Harm
            // - Demon Hunters: Metamorphosis, Fiery Brand, Demon Spikes
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(emergencyDefensive);

    // Tanking sequence
    auto tankingSequence = ::std::make_shared<BTSequence>("Tanking");

    tankingSequence->AddChild(::std::make_shared<BTCheckHasTarget>());
    tankingSequence->AddChild(::std::make_shared<BTCheckInRange>(0.0f, 10.0f));
    tankingSequence->AddChild(::std::make_shared<BTFaceTarget>());

    // Use threat generation
    tankingSequence->AddChild(::std::make_shared<BTAction>("GenerateThreat",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific threat generation abilities implementation required
            // When implementing per-class threat management, add logic here for:
            // - Warriors: Thunder Clap, Revenge, Shield Slam, Heroic Strike
            // - Paladins: Avenger's Shield, Hammer of the Righteous, Consecration
            // - Death Knights: Death and Decay, Heart Strike, Blood Boil
            // - Druids: Mangle, Thrash, Swipe, Maul
            // - Monks: Keg Smash, Breath of Fire, Tiger Palm
            // - Demon Hunters: Immolation Aura, Spirit Bomb, Fracture
            return BTStatus::SUCCESS;
        }
    ));

    // Maintain defensive stance/presence
    tankingSequence->AddChild(::std::make_shared<BTAction>("DefensiveStance",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific defensive stance/presence implementation required
            // When implementing per-class stance management, add logic here for:
            // - Warriors: Defensive Stance (if applicable)
            // - Paladins: Righteous Fury / Devotion Aura
            // - Death Knights: Blood Presence / Frost Presence
            // - Druids: Bear Form
            // - Monks: Stance of the Sturdy Ox
            // - Demon Hunters: Demon Spikes (active mitigation)
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(tankingSequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildSingleTargetHealingTree()
{
    auto root = ::std::make_shared<BTSelector>("SingleTargetHealingRoot");

    // Heal self if critical
    auto selfHealSequence = ::std::make_shared<BTSequence>("SelfHeal");
    selfHealSequence->AddChild(::std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    selfHealSequence->AddChild(::std::make_shared<BTAction>("HealSelf",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;
            bb.Set<Unit*>("HealTarget", bot);
            // DESIGN NOTE: Class-specific self-healing spell implementation required
            // When implementing per-class healing, add logic here for:
            // - Priests: Flash Heal, Renew, Desperate Prayer
            // - Paladins: Flash of Light, Holy Light, Word of Glory
            // - Druids: Rejuvenation, Regrowth, Swiftmend
            // - Shamans: Healing Surge, Healing Wave, Riptide
            // - Monks: Soothing Mist, Enveloping Mist, Expel Harm
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(selfHealSequence);

    // Heal ally sequence
    auto healAllySequence = ::std::make_shared<BTSequence>("HealAlly");

    // Find most wounded ally
    healAllySequence->AddChild(::std::make_shared<BTFindWoundedAlly>(0.80f));

    // Check in range
    healAllySequence->AddChild(::std::make_shared<BTCheckHealTargetInRange>(40.0f));

    // Check line of sight
    healAllySequence->AddChild(::std::make_shared<BTCheckHealTargetLoS>());

    // Select appropriate heal spell based on deficit
    healAllySequence->AddChild(::std::make_shared<BTAction>("SelectHealSpell",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            float targetHealthPct = bb.GetOr<float>("HealTargetHealthPct", 1.0f);

            // DESIGN NOTE: Class-specific heal spell selection implementation required
            // When implementing per-class healing priorities, add logic here for:
            // - Priests: Flash Heal (fast), Greater Heal (efficient), Renew (HoT)
            // - Paladins: Flash of Light (fast), Holy Light (efficient), Word of Glory (instant)
            // - Druids: Regrowth (fast), Healing Touch (efficient), Rejuvenation (HoT)
            // - Shamans: Healing Surge (fast), Healing Wave (efficient), Riptide (HoT)
            // - Monks: Surging Mist (fast), Soothing Mist (channel), Enveloping Mist (efficient)
            // For now, simple logic:
            uint32 spellId = 0;
            if (targetHealthPct < 0.3f)
                spellId = 1; // Fast heal
            else if (targetHealthPct < 0.6f)
                spellId = 2; // Medium heal
            else
                spellId = 3; // Efficient heal

            bb.Set<uint32>("SelectedHealSpell", spellId);
            return BTStatus::SUCCESS;
        }
    ));

    // Cast heal (placeholder)
    healAllySequence->AddChild(::std::make_shared<BTAction>("CastHeal",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Spell casting implementation required
            // When implementing spell casting, add logic here to:
            // - Retrieve the selected heal spell ID from blackboard
            // - Verify the spell is available and off cooldown
            // - Check mana/resource requirements
            // - Execute the spell cast using TrinityCore Spell API
            // - Handle cast interruption and failure cases
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(healAllySequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildGroupHealingTree()
{
    auto root = ::std::make_shared<BTSelector>("GroupHealingRoot");

    // Self heal if critical
    auto selfHealSequence = ::std::make_shared<BTSequence>("SelfHeal");
    selfHealSequence->AddChild(::std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    selfHealSequence->AddChild(::std::make_shared<BTAction>("HealSelf",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific self-healing spell implementation required
            // When implementing per-class healing, add logic here for:
            // - Priests: Flash Heal, Renew, Desperate Prayer
            // - Paladins: Flash of Light, Holy Light, Word of Glory
            // - Druids: Rejuvenation, Regrowth, Swiftmend
            // - Shamans: Healing Surge, Healing Wave, Riptide
            // - Monks: Soothing Mist, Enveloping Mist, Expel Harm
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(selfHealSequence);

    // AoE heal sequence
    auto aoeHealSequence = ::std::make_shared<BTSequence>("AoEHeal");

    // Check multiple wounded (3+ allies <80% health)
    aoeHealSequence->AddChild(::std::make_shared<BTCheckGroupNeedsAoEHeal>(0.80f, 3));

    // Check mana sufficient
    aoeHealSequence->AddChild(::std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.30f,
        BTCheckResourcePercent::Comparison::GREATER_THAN));

    // Cast AoE heal (placeholder)
    aoeHealSequence->AddChild(::std::make_shared<BTAction>("CastAoEHeal",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific AoE healing spell implementation required
            // When implementing per-class group healing, add logic here for:
            // - Priests: Circle of Healing, Prayer of Healing, Holy Nova
            // - Paladins: Light of Dawn, Holy Radiance, Beacon of Light
            // - Druids: Wild Growth, Efflorescence, Tranquility
            // - Shamans: Chain Heal, Healing Rain, Healing Tide Totem
            // - Monks: Revival, Essence Font, Chi Burst
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(aoeHealSequence);

    // Fallback: Single target heal
    root->AddChild(BuildSingleTargetHealingTree());

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildDispelPriorityTree()
{
    auto root = ::std::make_shared<BTSelector>("DispelPriority");

    // Dispel magic (high priority)
    auto dispelMagicSequence = ::std::make_shared<BTSequence>("DispelMagic");
    dispelMagicSequence->AddChild(::std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::MAGIC));
    dispelMagicSequence->AddChild(::std::make_shared<BTCastDispel>(0)); // Placeholder spell ID
    root->AddChild(dispelMagicSequence);

    // Dispel curse
    auto dispelCurseSequence = ::std::make_shared<BTSequence>("DispelCurse");
    dispelCurseSequence->AddChild(::std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::CURSE));
    dispelCurseSequence->AddChild(::std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelCurseSequence);

    // Dispel disease
    auto dispelDiseaseSequence = ::std::make_shared<BTSequence>("DispelDisease");
    dispelDiseaseSequence->AddChild(::std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::DISEASE));
    dispelDiseaseSequence->AddChild(::std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelDiseaseSequence);

    // Dispel poison
    auto dispelPoisonSequence = ::std::make_shared<BTSequence>("DispelPoison");
    dispelPoisonSequence->AddChild(::std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::POISON));
    dispelPoisonSequence->AddChild(::std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelPoisonSequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildFollowLeaderTree()
{
    auto root = ::std::make_shared<BTSequence>("FollowLeader");

    // Check not in combat
    root->AddChild(::std::make_shared<BTInverter>("NotInCombat",
        ::std::make_shared<BTCheckInCombat>()
    ));

    // Check too far from leader (>10 yards)
    root->AddChild(::std::make_shared<BTCondition>("TooFarFromLeader",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;

            Player* bot = ai->GetBot();
            if (!bot) return false;

            Player* leader = nullptr;
            Group* group = bot->GetGroup();
            if (group)
            {
                ObjectGuid leaderGuid = group->GetLeaderGUID();
                leader = ObjectAccessor::FindPlayer(leaderGuid);
            }

            if (!leader) return false;

            return bot->GetDistance(leader) > 10.0f;
        }
    ));

    // Move to leader
    root->AddChild(::std::make_shared<BTFollowLeader>(5.0f));

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildCombatPositioningTree()
{
    auto root = ::std::make_shared<BTSelector>("CombatPositioning");

    // Melee positioning
    auto meleeSequence = ::std::make_shared<BTSequence>("MeleePositioning");
    meleeSequence->AddChild(::std::make_shared<BTCondition>("IsMeleeClass",
        [](BotAI* ai, BTBlackboard& bb) {
            // DESIGN NOTE: Condition check for melee class identification
            // Returns true as default behavior
            // Full implementation should evaluate:
            // - Bot's class type (Warrior, Rogue, Death Knight, Paladin, Monk, etc.)
            // - Current specialization (Arms, Fury, Assassination, Subtlety, Unholy, Windwalker, etc.)
            // - Determine if specialization is melee-based combat
            // - Consider hybrid classes (Enhancement Shaman, Feral Druid, Retribution Paladin)
            // Reference: See ClassAI implementations for spec-specific logic
            return true;
        }
    ));
    meleeSequence->AddChild(::std::make_shared<BTCondition>("TooFarFromTarget",
        [](BotAI* ai, BTBlackboard& bb) {
            Unit* target = nullptr;
            if (!bb.Get<Unit*>("CurrentTarget", target) || !target)
                return false;

            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            return bot->GetDistance(target) > 5.0f;
        }
    ));
    meleeSequence->AddChild(::std::make_shared<BTMoveToTarget>(0.0f, 5.0f));
    root->AddChild(meleeSequence);

    // Ranged positioning
    auto rangedSequence = ::std::make_shared<BTSequence>("RangedPositioning");
    rangedSequence->AddChild(::std::make_shared<BTCondition>("IsRangedClass",
        [](BotAI* ai, BTBlackboard& bb) {
            // DESIGN NOTE: Condition check for ranged class identification
            // Returns true as default behavior
            // Full implementation should evaluate:
            // - Bot's class type (Hunter, Mage, Warlock, Priest, Shaman, Druid, etc.)
            // - Current specialization (Marksmanship, Beast Mastery, Arcane, Fire, Shadow, Elemental, Balance)
            // - Determine if specialization is ranged-based combat
            // - Consider hybrid classes (Balance Druid, Elemental Shaman, Shadow Priest)
            // Reference: See ClassAI implementations for spec-specific logic
            return true;
        }
    ));
    rangedSequence->AddChild(::std::make_shared<BTCondition>("NotInOptimalRange",
        [](BotAI* ai, BTBlackboard& bb) {
            Unit* target = nullptr;
            if (!bb.Get<Unit*>("CurrentTarget", target) || !target)
                return false;

            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            float distance = bot->GetDistance(target);
            return distance < 20.0f || distance > 35.0f;
        }
    ));
    rangedSequence->AddChild(::std::make_shared<BTMoveToTarget>(20.0f, 35.0f));
    root->AddChild(rangedSequence);

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildFleeToSafetyTree()
{
    auto root = ::std::make_shared<BTSequence>("FleeToSafety");

    // Check health below 20%
    root->AddChild(::std::make_shared<BTCheckHealthPercent>(0.20f, BTCheckHealthPercent::Comparison::LESS_THAN));

    // Check in combat
    root->AddChild(::std::make_shared<BTCheckInCombat>());

    // Find safe position
    root->AddChild(::std::make_shared<BTFindSafePosition>(20.0f));

    // Move to safe position
    root->AddChild(::std::make_shared<BTMoveToPosition>(2.0f));

    // Stop movement when safe
    root->AddChild(::std::make_shared<BTStopMovement>());

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildBuffMaintenanceTree()
{
    auto root = ::std::make_shared<BTSequence>("BuffMaintenance");

    // Check not in combat OR combat duration < 5 seconds
    auto canBuffSelector = ::std::make_shared<BTSelector>("CanBuff");
    canBuffSelector->AddChild(::std::make_shared<BTInverter>("NotInCombat",
        ::std::make_shared<BTCheckInCombat>()
    ));
    canBuffSelector->AddChild(::std::make_shared<BTCondition>("EarlyCombat",
        [](BotAI* ai, BTBlackboard& bb) {
            // DESIGN NOTE: Condition check for early combat phase
            // Returns false as default behavior
            // Full implementation should evaluate:
            // - Combat start timestamp (first combat state change)
            // - Current combat duration (current time - start time)
            // - Store combat start time in blackboard or bot state
            // - Return true if combat duration < 5 seconds
            // Reference: See BotAI combat state tracking for timestamp management
            return false;
        }
    ));
    root->AddChild(canBuffSelector);

    // Check missing self buff
    root->AddChild(::std::make_shared<BTCondition>("MissingBuff",
        [](BotAI* ai, BTBlackboard& bb) {
            // DESIGN NOTE: Condition check for missing class buffs
            // Returns false as default behavior
            // Full implementation should evaluate:
            // - Warriors: Battle Shout, Commanding Shout
            // - Mages: Arcane Intellect, Mage Armor
            // - Paladins: Blessings (Might, Kings, Wisdom), Seals
            // - Priests: Power Word: Fortitude, Inner Fire
            // - Druids: Mark of the Wild, appropriate form
            // - Warlocks: Soul Link, Demon Armor/Fel Armor
            // - Shamans: Water Shield/Lightning Shield, totems
            // - Death Knights: Bone Shield, presence buffs
            // - Monks: Legacy of the Emperor/White Tiger
            // Reference: See ClassAI implementations for spec-specific logic
            return false;
        }
    ));

    // Cast self buff
    root->AddChild(::std::make_shared<BTAction>("CastBuff",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Class-specific buff casting implementation required
            // When implementing per-class buff casting, add logic here to:
            // - Determine which buff is missing (from previous check)
            // - Select appropriate spell ID for the buff
            // - Verify spell is available and ready to cast
            // - Cast the buff on self or appropriate party members
            // - Handle cast failures and cooldowns
            return BTStatus::SUCCESS;
        }
    ));

    return root;
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::BuildResourceManagementTree()
{
    auto root = ::std::make_shared<BTSelector>("ResourceManagement");

    // Drink/Eat sequence
    auto consumableSequence = ::std::make_shared<BTSequence>("UseConsumable");

    // Check not in combat
    consumableSequence->AddChild(::std::make_shared<BTInverter>("NotInCombat",
        ::std::make_shared<BTCheckInCombat>()
    ));

    // Check mana < 30%
    consumableSequence->AddChild(::std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.30f,
        BTCheckResourcePercent::Comparison::LESS_THAN));

    // Use consumable
    consumableSequence->AddChild(::std::make_shared<BTAction>("UseConsumable",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // DESIGN NOTE: Consumable item usage implementation required
            // When implementing consumable usage, add logic here to:
            // - Search bot's inventory for appropriate consumables (mana/health potions, food, drink)
            // - Prioritize by quality/effectiveness (conjured > vendor items)
            // - Check item cooldowns and usage restrictions
            // - Use the item via TrinityCore Item API
            // - Handle special cases (conjured items, class-specific consumables)
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(consumableSequence);

    // Conserve mana sequence
    auto conserveSequence = ::std::make_shared<BTSequence>("ConserveMana");

    // Check mana < 50%
    conserveSequence->AddChild(::std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.50f,
        BTCheckResourcePercent::Comparison::LESS_THAN));

    // Avoid expensive spells
    conserveSequence->AddChild(::std::make_shared<BTAction>("AvoidExpensiveSpells",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            bb.Set<bool>("ConserveMana", true);
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(conserveSequence);

    return root;
}

void BehaviorTreeFactory::RegisterCustomTree(::std::string const& name,
    ::std::function<::std::shared_ptr<BTNode>()> builder)
{
    _customTreeBuilders[name] = builder;
    TC_LOG_INFO("playerbot.bt", "Registered custom behavior tree: {}", name);
}

::std::shared_ptr<BTNode> BehaviorTreeFactory::CreateCustomTree(::std::string const& name)
{
    auto it = _customTreeBuilders.find(name);
    if (it != _customTreeBuilders.end())
    {
        return it->second();
    }

    TC_LOG_ERROR("playerbot.bt", "Custom tree '{}' not found in registry", name);
    return nullptr;
}

} // namespace Playerbot
