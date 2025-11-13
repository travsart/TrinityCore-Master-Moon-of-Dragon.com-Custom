/*
 * Copyright (C) 2024+ TrinityCore <http://www.trinitycore.org/>
 *
 * Phase 5 Enhancement: Behavior Tree Implementation
 */

#include "BehaviorTree.h"
#include "DecisionFusionSystem.h"
#include "Player.h"
#include "Unit.h"
#include "Log.h"

namespace bot { namespace ai {

// ============================================================================
// BehaviorTree Implementation
// ============================================================================

BehaviorTree::BehaviorTree(const std::string& name)
    : _name(name)
    , _root(nullptr)
    , _lastStatus(NodeStatus::FAILURE)
    , _debugLogging(false)
    , _recommendedAction(0)
    , _recommendedTarget(nullptr)
{
}

void BehaviorTree::SetRoot(std::shared_ptr<BehaviorNode> root)
{
    _root = root;
}

NodeStatus BehaviorTree::Tick(Player* bot, Unit* target)
{

    if (_debugLogging)
    {
        TC_LOG_DEBUG("playerbot", "BehaviorTree: {} executing tick on bot {}",
            _name, bot->GetName());
    }

    // Execute the tree
    _lastStatus = _root->Tick(bot, target);

    if (_debugLogging)
    {
        const char* statusStr = "UNKNOWN";
        switch (_lastStatus)
        {
            case NodeStatus::SUCCESS: statusStr = "SUCCESS"; break;
            case NodeStatus::FAILURE: statusStr = "FAILURE"; break;
            case NodeStatus::RUNNING: statusStr = "RUNNING"; break;
        }
        TC_LOG_DEBUG("playerbot", "BehaviorTree: {} returned {}", _name, statusStr);
    }

    return _lastStatus;
}

void BehaviorTree::Reset()
{
    if (_root)
        _root->Reset();

    _lastStatus = NodeStatus::FAILURE;
    _recommendedAction = 0;
    _recommendedTarget = nullptr;
}

DecisionVote BehaviorTree::GetVote(Player* bot, Unit* target, CombatContext context) const
{
    DecisionVote vote;
    vote.source = DecisionSource::BEHAVIOR_TREE;

    // If tree has no root or last status was failure, no vote
    if (!_root || _lastStatus == NodeStatus::FAILURE)
        return vote;

    // Use recommended action if available
    vote.actionId = _recommendedAction;
    vote.target = _recommendedTarget;

    // Confidence based on tree status
    // RUNNING trees have moderate confidence (they're still evaluating)
    // SUCCESS trees have high confidence (they completed successfully)
    if (_lastStatus == NodeStatus::RUNNING)
        vote.confidence = 0.7f;
    else if (_lastStatus == NodeStatus::SUCCESS)
        vote.confidence = 0.9f;
    else
        vote.confidence = 0.0f;

    // Urgency based on context
    // Behavior trees represent strategic decisions, so urgency is moderate
    switch (context)
    {
        case CombatContext::RAID_HEROIC:
        case CombatContext::DUNGEON_BOSS:
            vote.urgency = 0.8f;  // High urgency in challenging content
            break;
        case CombatContext::PVP_ARENA:
            vote.urgency = 0.85f; // Very high urgency in PvP
            break;
        default:
            vote.urgency = 0.6f;  // Moderate urgency
            break;
    }

    // Set reasoning
    vote.reasoning = "BehaviorTree: " + _name;

    return vote;
}

// ============================================================================
// Helper Functions for Node Creation
// ============================================================================

namespace BehaviorTreeBuilder
{
    /**
     * @brief Create a sequence node with children
     */
    std::shared_ptr<SequenceNode> Sequence(const std::string& name,
        std::initializer_list<std::shared_ptr<BehaviorNode>> children)
    {
        auto seq = std::make_shared<SequenceNode>(name);
        for (auto& child : children)
            seq->AddChild(child);
        return seq;
    }

    /**
     * @brief Create a selector node with children
     */
    std::shared_ptr<SelectorNode> Selector(const std::string& name,
        std::initializer_list<std::shared_ptr<BehaviorNode>> children)
    {
        auto sel = std::make_shared<SelectorNode>(name);
        for (auto& child : children)
            sel->AddChild(child);
        return sel;
    }

    /**
     * @brief Create a condition node
     */
    std::shared_ptr<ConditionNode> Condition(const std::string& name,
        std::function<bool(Player*, Unit*)> condition)
    {
        return std::make_shared<ConditionNode>(name, condition);
    }

    /**
     * @brief Create an action node
     */
    std::shared_ptr<ActionNode> Action(const std::string& name,
        std::function<NodeStatus(Player*, Unit*)> action)
    {
        return std::make_shared<ActionNode>(name, action);
    }

    /**
     * @brief Create an inverter node
     */
    std::shared_ptr<InverterNode> Inverter(const std::string& name,
        std::shared_ptr<BehaviorNode> child)
    {
        return std::make_shared<InverterNode>(name, child);
    }

    /**
     * @brief Create a repeater node
     */
    std::shared_ptr<RepeaterNode> Repeater(const std::string& name,
        std::shared_ptr<BehaviorNode> child, uint32 maxRepeats = 0)
    {
        return std::make_shared<RepeaterNode>(name, child, maxRepeats);
    }
}

// ============================================================================
// Example: Healer Behavior Tree
// ============================================================================

std::shared_ptr<BehaviorTree> CreateHealerBehaviorTree()
{
    using namespace BehaviorTreeBuilder;

    auto tree = std::make_shared<BehaviorTree>("Healer");

    // Root selector: Try emergency → tank → DPS → maintain HoTs
    auto root = Selector("Root", {

        // Emergency: Self heal if very low
        Sequence("Emergency Self Heal", {
            Condition("Self HP < 30%", [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 30.0f;
            }),
            Action("Cast Flash Heal on Self", [](Player* bot, Unit*) {
                // Cast emergency heal on self
                // Note: Actual spell casting would be done via ClassAI
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Emergency self heal for {}",
                    bot->GetName());
                return NodeStatus::SUCCESS;
            })
        }),

        // Tank healing: Prioritize tank below 60%
        Sequence("Tank Heal", {
            Condition("Tank HP < 60%", [](Player* bot, Unit* target) {
                // Check if target is tank and below 60%
                // Note: Tank identification would use group role system
                return target && target->GetHealthPct() < 60.0f;
            }),
            Action("Cast Greater Heal on Tank", [](Player* bot, Unit* target) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Healing tank {}",
                    target->GetName());
                return NodeStatus::SUCCESS;
            })
        }),

        // DPS healing: Heal DPS below 50%
        Sequence("DPS Heal", {
            Condition("DPS HP < 50%", [](Player* bot, Unit* target) {
                return target && target->GetHealthPct() < 50.0f;
            }),
            Action("Cast Flash Heal on DPS", [](Player* bot, Unit* target) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Healing DPS {}",
                    target->GetName());
                return NodeStatus::SUCCESS;
            })
        }),

        // Maintenance: Keep HoTs up
        Action("Maintain HoTs", [](Player* bot, Unit*) {
            TC_LOG_DEBUG("playerbot", "BehaviorTree: Maintaining HoTs");
            return NodeStatus::SUCCESS;
        })
    });

    tree->SetRoot(root);
    return tree;
}

// ============================================================================
// Example: Tank Behavior Tree
// ============================================================================

std::shared_ptr<BehaviorTree> CreateTankBehaviorTree()
{
    using namespace BehaviorTreeBuilder;

    auto tree = std::make_shared<BehaviorTree>("Tank");

    // Root selector: Try defensive → taunt → threat → damage
    auto root = Selector("Root", {

        // Emergency defensive if very low HP
        Sequence("Emergency Defensive", {
            Condition("HP < 20%", [](Player* bot, Unit*) {
                return bot && bot->GetHealthPct() < 20.0f;
            }),
            Action("Use Emergency Defensive", [](Player* bot, Unit*) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Emergency defensive for {}",
                    bot->GetName());
                return NodeStatus::SUCCESS;
            })
        }),

        // Taunt if enemy is not targeting us
        Sequence("Taunt Enemy", {
            Condition("Enemy not on us", [](Player* bot, Unit* target) {
                return target && target->GetVictim() != bot;
            }),
            Action("Cast Taunt", [](Player* bot, Unit* target) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Taunting {} for {}",
                    target->GetName(), bot->GetName());
                return NodeStatus::SUCCESS;
            })
        }),

        // Build threat with threat abilities
        Action("Build Threat", [](Player* bot, Unit* target) {
            TC_LOG_DEBUG("playerbot", "BehaviorTree: Building threat");
            return NodeStatus::SUCCESS;
        }),

        // Fill with damage abilities
        Action("Deal Damage", [](Player* bot, Unit* target) {
            TC_LOG_DEBUG("playerbot", "BehaviorTree: Dealing damage");
            return NodeStatus::SUCCESS;
        })
    });

    tree->SetRoot(root);
    return tree;
}

// ============================================================================
// Example: DPS Behavior Tree
// ============================================================================

std::shared_ptr<BehaviorTree> CreateDPSBehaviorTree()
{
    using namespace BehaviorTreeBuilder;

    auto tree = std::make_shared<BehaviorTree>("DPS");

    // Root selector: Try cooldowns → AoE → single target → filler
    auto root = Selector("Root", {

        // Use offensive cooldowns if available and target > 50% HP
        Sequence("Offensive Cooldowns", {
            Condition("Target HP > 50%", [](Player* bot, Unit* target) {
                return target && target->GetHealthPct() > 50.0f;
            }),
            Condition("Cooldowns Available", [](Player* bot, Unit*) {
                if (!bot)
                    return false;

                // Check if we're in combat and not recently used cooldowns
                // This is a simplified check for the demo tree
                // Real implementations in Refactored specs check specific cooldowns
                return bot->IsInCombat();
            }),
            Action("Use Offensive Cooldown", [](Player* bot, Unit*) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Using offensive cooldown");
                return NodeStatus::SUCCESS;
            })
        }),

        // AoE if 3+ enemies
        Sequence("AoE Rotation", {
            Condition("3+ Enemies", [](Player* bot, Unit* target) {
                if (!bot || !target)
                    return false;

                // Count nearby enemies within 10 yards
                uint32 enemyCount = 0;
                std::list<Creature*> creatures;
                Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 10.0f);
                Trinity::CreatureListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, creatures, check);
                Cell::VisitGridObjects(bot, searcher, 10.0f);

                for (Creature* creature : creatures)
                {
                    if (creature && bot->IsValidAttackTarget(creature))
                        enemyCount++;
                }

                return enemyCount >= 3;
            }),
            Action("Cast AoE Spell", [](Player* bot, Unit*) {
                TC_LOG_DEBUG("playerbot", "BehaviorTree: Casting AoE");
                return NodeStatus::SUCCESS;
            })
        }),

        // Single target rotation
        Action("Single Target Rotation", [](Player* bot, Unit* target) {
            TC_LOG_DEBUG("playerbot", "BehaviorTree: Single target rotation");
            return NodeStatus::SUCCESS;
        }),

        // Filler spell
        Action("Filler Spell", [](Player* bot, Unit* target) {
            TC_LOG_DEBUG("playerbot", "BehaviorTree: Casting filler");
            return NodeStatus::SUCCESS;
        })
    });

    tree->SetRoot(root);
    return tree;
}

}} // namespace bot::ai
