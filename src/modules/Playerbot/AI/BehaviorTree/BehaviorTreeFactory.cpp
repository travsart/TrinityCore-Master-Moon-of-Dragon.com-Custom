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
std::unordered_map<std::string, std::function<std::shared_ptr<BTNode>()>> BehaviorTreeFactory::_customTreeBuilders;

std::shared_ptr<BTNode> BehaviorTreeFactory::CreateTree(TreeType type)
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

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildMeleeCombatTree()
{
    // Root: Selector (try combat or flee)
    auto root = std::make_shared<BTSelector>("MeleeCombatRoot");

    // Branch 1: Flee if critically wounded
    auto fleeCondition = std::make_shared<BTCondition>("CriticalHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;
            return bot->GetHealthPct() < 20.0f && bot->IsInCombat();
        }
    );
    root->AddChild(fleeCondition);

    // Branch 2: Melee combat sequence
    auto combatSequence = std::make_shared<BTSequence>("MeleeCombat");

    // Check has target
    combatSequence->AddChild(std::make_shared<BTCheckHasTarget>());

    // Check in melee range (0-5 yards)
    combatSequence->AddChild(std::make_shared<BTCheckInRange>(0.0f, 5.0f));

    // Use defensive cooldowns if low health
    auto defensiveSelector = std::make_shared<BTSelector>("DefensiveCooldowns");
    defensiveSelector->AddChild(std::make_shared<BTCondition>("HighHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            return bot && bot->GetHealthPct() > 50.0f;
        }
    ));
    // If low health, try to use defensive CD (placeholder - class-specific)
    defensiveSelector->AddChild(std::make_shared<BTAction>("UseDefensive",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Class-specific defensive cooldowns
            return BTStatus::SUCCESS;
        }
    ));
    combatSequence->AddChild(defensiveSelector);

    // Face target
    combatSequence->AddChild(std::make_shared<BTFaceTarget>());

    // Execute melee attack
    combatSequence->AddChild(std::make_shared<BTMeleeAttack>());

    root->AddChild(combatSequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildRangedCombatTree()
{
    auto root = std::make_shared<BTSelector>("RangedCombatRoot");

    // Flee if critically wounded
    auto fleeSequence = std::make_shared<BTSequence>("FleeIfCritical");
    fleeSequence->AddChild(std::make_shared<BTCondition>("CriticalHealth",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            return bot && bot->GetHealthPct() < 20.0f && bot->IsInCombat();
        }
    ));
    fleeSequence->AddChild(std::make_shared<BTAction>("StartFleeing",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            bb.Set<bool>("ShouldFlee", true);
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(fleeSequence);

    // Ranged combat sequence
    auto combatSequence = std::make_shared<BTSequence>("RangedCombat");

    combatSequence->AddChild(std::make_shared<BTCheckHasTarget>());

    // Check in ranged range (5-40 yards)
    auto rangeCheck = std::make_shared<BTSelector>("RangeManagement");
    rangeCheck->AddChild(std::make_shared<BTCheckInRange>(5.0f, 40.0f));

    // If not in range, move to optimal range
    auto moveSequence = std::make_shared<BTSequence>("MoveToRange");
    moveSequence->AddChild(std::make_shared<BTCondition>("OutOfRange",
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
    moveSequence->AddChild(std::make_shared<BTMoveToTarget>(20.0f, 35.0f));
    rangeCheck->AddChild(moveSequence);

    combatSequence->AddChild(rangeCheck);

    // Face target
    combatSequence->AddChild(std::make_shared<BTFaceTarget>());

    // Cast ranged abilities (placeholder - class-specific)
    combatSequence->AddChild(std::make_shared<BTAction>("CastRangedSpell",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Class-specific ranged spell rotation
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(combatSequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildTankCombatTree()
{
    auto root = std::make_shared<BTSelector>("TankCombatRoot");

    // Use defensive CDs if critical
    auto emergencyDefensive = std::make_shared<BTSequence>("EmergencyDefensive");
    emergencyDefensive->AddChild(std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    emergencyDefensive->AddChild(std::make_shared<BTCheckInCombat>());
    emergencyDefensive->AddChild(std::make_shared<BTAction>("UseEmergencyCD",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Class-specific emergency defensive
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(emergencyDefensive);

    // Tanking sequence
    auto tankingSequence = std::make_shared<BTSequence>("Tanking");

    tankingSequence->AddChild(std::make_shared<BTCheckHasTarget>());
    tankingSequence->AddChild(std::make_shared<BTCheckInRange>(0.0f, 10.0f));
    tankingSequence->AddChild(std::make_shared<BTFaceTarget>());

    // Use threat generation
    tankingSequence->AddChild(std::make_shared<BTAction>("GenerateThreat",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Class-specific threat abilities
            return BTStatus::SUCCESS;
        }
    ));

    // Maintain defensive stance/presence
    tankingSequence->AddChild(std::make_shared<BTAction>("DefensiveStance",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Check and apply defensive stance
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(tankingSequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildSingleTargetHealingTree()
{
    auto root = std::make_shared<BTSelector>("SingleTargetHealingRoot");

    // Heal self if critical
    auto selfHealSequence = std::make_shared<BTSequence>("SelfHeal");
    selfHealSequence->AddChild(std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    selfHealSequence->AddChild(std::make_shared<BTAction>("HealSelf",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;
            bb.Set<Unit*>("HealTarget", bot);
            // TODO: Cast self-heal spell
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(selfHealSequence);

    // Heal ally sequence
    auto healAllySequence = std::make_shared<BTSequence>("HealAlly");

    // Find most wounded ally
    healAllySequence->AddChild(std::make_shared<BTFindWoundedAlly>(0.80f));

    // Check in range
    healAllySequence->AddChild(std::make_shared<BTCheckHealTargetInRange>(40.0f));

    // Check line of sight
    healAllySequence->AddChild(std::make_shared<BTCheckHealTargetLoS>());

    // Select appropriate heal spell based on deficit
    healAllySequence->AddChild(std::make_shared<BTAction>("SelectHealSpell",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            float targetHealthPct = bb.GetOr<float>("HealTargetHealthPct", 1.0f);

            // TODO: Class-specific heal selection
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
    healAllySequence->AddChild(std::make_shared<BTAction>("CastHeal",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Actually cast the selected heal spell
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(healAllySequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildGroupHealingTree()
{
    auto root = std::make_shared<BTSelector>("GroupHealingRoot");

    // Self heal if critical
    auto selfHealSequence = std::make_shared<BTSequence>("SelfHeal");
    selfHealSequence->AddChild(std::make_shared<BTCheckHealthPercent>(0.30f, BTCheckHealthPercent::Comparison::LESS_THAN));
    selfHealSequence->AddChild(std::make_shared<BTAction>("HealSelf",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Self-heal
            return BTStatus::SUCCESS;
        }
    ));
    root->AddChild(selfHealSequence);

    // AoE heal sequence
    auto aoeHealSequence = std::make_shared<BTSequence>("AoEHeal");

    // Check multiple wounded (3+ allies <80% health)
    aoeHealSequence->AddChild(std::make_shared<BTCheckGroupNeedsAoEHeal>(0.80f, 3));

    // Check mana sufficient
    aoeHealSequence->AddChild(std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.30f,
        BTCheckResourcePercent::Comparison::GREATER_THAN));

    // Cast AoE heal (placeholder)
    aoeHealSequence->AddChild(std::make_shared<BTAction>("CastAoEHeal",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Class-specific AoE heal (Chain Heal, Circle of Healing, etc.)
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(aoeHealSequence);

    // Fallback: Single target heal
    root->AddChild(BuildSingleTargetHealingTree());

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildDispelPriorityTree()
{
    auto root = std::make_shared<BTSelector>("DispelPriority");

    // Dispel magic (high priority)
    auto dispelMagicSequence = std::make_shared<BTSequence>("DispelMagic");
    dispelMagicSequence->AddChild(std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::MAGIC));
    dispelMagicSequence->AddChild(std::make_shared<BTCastDispel>(0)); // Placeholder spell ID
    root->AddChild(dispelMagicSequence);

    // Dispel curse
    auto dispelCurseSequence = std::make_shared<BTSequence>("DispelCurse");
    dispelCurseSequence->AddChild(std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::CURSE));
    dispelCurseSequence->AddChild(std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelCurseSequence);

    // Dispel disease
    auto dispelDiseaseSequence = std::make_shared<BTSequence>("DispelDisease");
    dispelDiseaseSequence->AddChild(std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::DISEASE));
    dispelDiseaseSequence->AddChild(std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelDiseaseSequence);

    // Dispel poison
    auto dispelPoisonSequence = std::make_shared<BTSequence>("DispelPoison");
    dispelPoisonSequence->AddChild(std::make_shared<BTFindDispelTarget>(BTFindDispelTarget::DispelType::POISON));
    dispelPoisonSequence->AddChild(std::make_shared<BTCastDispel>(0));
    root->AddChild(dispelPoisonSequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildFollowLeaderTree()
{
    auto root = std::make_shared<BTSequence>("FollowLeader");

    // Check not in combat
    root->AddChild(std::make_shared<BTInverter>("NotInCombat",
        std::make_shared<BTCheckInCombat>()
    ));

    // Check too far from leader (>10 yards)
    root->AddChild(std::make_shared<BTCondition>("TooFarFromLeader",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;

            Player* bot = ai->GetBot();
            if (!bot) return false;

            Player* leader = nullptr;
            Group* group = bot->GetGroup();
            if (group) {
                ObjectGuid leaderGuid = group->GetLeaderGUID();
                leader = ObjectAccessor::FindPlayer(leaderGuid);
            }

            if (!leader) return false;

            return bot->GetDistance(leader) > 10.0f;
        }
    ));

    // Move to leader
    root->AddChild(std::make_shared<BTFollowLeader>(5.0f));

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildCombatPositioningTree()
{
    auto root = std::make_shared<BTSelector>("CombatPositioning");

    // Melee positioning
    auto meleeSequence = std::make_shared<BTSequence>("MeleePositioning");
    meleeSequence->AddChild(std::make_shared<BTCondition>("IsMeleeClass",
        [](BotAI* ai, BTBlackboard& bb) {
            // TODO: Check if bot is melee class
            return true; // Placeholder
        }
    ));
    meleeSequence->AddChild(std::make_shared<BTCondition>("TooFarFromTarget",
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
    meleeSequence->AddChild(std::make_shared<BTMoveToTarget>(0.0f, 5.0f));
    root->AddChild(meleeSequence);

    // Ranged positioning
    auto rangedSequence = std::make_shared<BTSequence>("RangedPositioning");
    rangedSequence->AddChild(std::make_shared<BTCondition>("IsRangedClass",
        [](BotAI* ai, BTBlackboard& bb) {
            // TODO: Check if bot is ranged class
            return true; // Placeholder
        }
    ));
    rangedSequence->AddChild(std::make_shared<BTCondition>("NotInOptimalRange",
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
    rangedSequence->AddChild(std::make_shared<BTMoveToTarget>(20.0f, 35.0f));
    root->AddChild(rangedSequence);

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildFleeToSafetyTree()
{
    auto root = std::make_shared<BTSequence>("FleeToSafety");

    // Check health below 20%
    root->AddChild(std::make_shared<BTCheckHealthPercent>(0.20f, BTCheckHealthPercent::Comparison::LESS_THAN));

    // Check in combat
    root->AddChild(std::make_shared<BTCheckInCombat>());

    // Find safe position
    root->AddChild(std::make_shared<BTFindSafePosition>(20.0f));

    // Move to safe position
    root->AddChild(std::make_shared<BTMoveToPosition>(2.0f));

    // Stop movement when safe
    root->AddChild(std::make_shared<BTStopMovement>());

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildBuffMaintenanceTree()
{
    auto root = std::make_shared<BTSequence>("BuffMaintenance");

    // Check not in combat OR combat duration < 5 seconds
    root->AddChild(std::make_shared<BTSelector>("CanBuff",
        std::make_shared<BTInverter>("NotInCombat",
            std::make_shared<BTCheckInCombat>()
        ),
        std::make_shared<BTCondition>("EarlyCombat",
            [](BotAI* ai, BTBlackboard& bb) {
                // TODO: Check combat duration
                return false; // Placeholder
            }
        )
    ));

    // Check missing self buff
    root->AddChild(std::make_shared<BTCondition>("MissingBuff",
        [](BotAI* ai, BTBlackboard& bb) {
            // TODO: Check for missing class buffs
            return false; // Placeholder
        }
    ));

    // Cast self buff
    root->AddChild(std::make_shared<BTAction>("CastBuff",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Cast appropriate buff
            return BTStatus::SUCCESS;
        }
    ));

    return root;
}

std::shared_ptr<BTNode> BehaviorTreeFactory::BuildResourceManagementTree()
{
    auto root = std::make_shared<BTSelector>("ResourceManagement");

    // Drink/Eat sequence
    auto consumableSequence = std::make_shared<BTSequence>("UseConsumable");

    // Check not in combat
    consumableSequence->AddChild(std::make_shared<BTInverter>("NotInCombat",
        std::make_shared<BTCheckInCombat>()
    ));

    // Check mana < 30%
    consumableSequence->AddChild(std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.30f,
        BTCheckResourcePercent::Comparison::LESS_THAN));

    // Use consumable
    consumableSequence->AddChild(std::make_shared<BTAction>("UseConsumable",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            // TODO: Find and use mana consumable
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(consumableSequence);

    // Conserve mana sequence
    auto conserveSequence = std::make_shared<BTSequence>("ConserveMana");

    // Check mana < 50%
    conserveSequence->AddChild(std::make_shared<BTCheckResourcePercent>(POWER_MANA, 0.50f,
        BTCheckResourcePercent::Comparison::LESS_THAN));

    // Avoid expensive spells
    conserveSequence->AddChild(std::make_shared<BTAction>("AvoidExpensiveSpells",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            bb.Set<bool>("ConserveMana", true);
            return BTStatus::SUCCESS;
        }
    ));

    root->AddChild(conserveSequence);

    return root;
}

void BehaviorTreeFactory::RegisterCustomTree(std::string const& name,
    std::function<std::shared_ptr<BTNode>()> builder)
{
    _customTreeBuilders[name] = builder;
    TC_LOG_INFO("playerbot.bt", "Registered custom behavior tree: {}", name);
}

std::shared_ptr<BTNode> BehaviorTreeFactory::CreateCustomTree(std::string const& name)
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
