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
    // If low health, try to use defensive CD (class-specific)
    defensiveSelector->AddChild(::std::make_shared<BTAction>("UseDefensive",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;
            uint32 defSpellId = 0;

            switch (playerClass)
            {
                case CLASS_WARRIOR:
                    // Die By the Sword (Arms/Fury), Ignore Pain (Prot), Rallying Cry
                    if (specId == 71 || specId == 72) // Arms/Fury
                        defSpellId = bot->HasSpell(118038) && !bot->GetSpellHistory()->HasCooldown(118038) ? 118038 : 0; // Die By the Sword
                    else
                        defSpellId = bot->HasSpell(190456) && !bot->GetSpellHistory()->HasCooldown(190456) ? 190456 : 0; // Ignore Pain
                    break;

                case CLASS_PALADIN:
                    // Divine Shield > Divine Protection > Word of Glory
                    if (bot->HasSpell(642) && !bot->GetSpellHistory()->HasCooldown(642))
                        defSpellId = 642; // Divine Shield
                    else if (bot->HasSpell(498) && !bot->GetSpellHistory()->HasCooldown(498))
                        defSpellId = 498; // Divine Protection
                    break;

                case CLASS_DEATH_KNIGHT:
                    // Icebound Fortitude > Anti-Magic Shell
                    if (bot->HasSpell(48792) && !bot->GetSpellHistory()->HasCooldown(48792))
                        defSpellId = 48792; // Icebound Fortitude
                    else if (bot->HasSpell(48707) && !bot->GetSpellHistory()->HasCooldown(48707))
                        defSpellId = 48707; // Anti-Magic Shell
                    break;

                case CLASS_ROGUE:
                    // Evasion > Cloak of Shadows > Crimson Vial
                    if (bot->HasSpell(5277) && !bot->GetSpellHistory()->HasCooldown(5277))
                        defSpellId = 5277; // Evasion
                    else if (bot->HasSpell(31224) && !bot->GetSpellHistory()->HasCooldown(31224))
                        defSpellId = 31224; // Cloak of Shadows
                    else if (bot->HasSpell(185311) && !bot->GetSpellHistory()->HasCooldown(185311))
                        defSpellId = 185311; // Crimson Vial
                    break;

                case CLASS_DRUID:
                    // Barkskin > Survival Instincts
                    if (bot->HasSpell(22812) && !bot->GetSpellHistory()->HasCooldown(22812))
                        defSpellId = 22812; // Barkskin
                    else if (specId == 103 || specId == 104) // Feral/Guardian
                    {
                        if (bot->HasSpell(61336) && !bot->GetSpellHistory()->HasCooldown(61336))
                            defSpellId = 61336; // Survival Instincts
                    }
                    break;

                case CLASS_MONK:
                    // Touch of Karma (WW) > Fortifying Brew > Diffuse Magic
                    if (specId == 269 && bot->HasSpell(122470) && !bot->GetSpellHistory()->HasCooldown(122470))
                        defSpellId = 122470; // Touch of Karma
                    else if (bot->HasSpell(115203) && !bot->GetSpellHistory()->HasCooldown(115203))
                        defSpellId = 115203; // Fortifying Brew
                    else if (bot->HasSpell(122783) && !bot->GetSpellHistory()->HasCooldown(122783))
                        defSpellId = 122783; // Diffuse Magic
                    break;

                case CLASS_DEMON_HUNTER:
                    // Blur (Havoc) > Netherwalk
                    if (specId == 577 && bot->HasSpell(198589) && !bot->GetSpellHistory()->HasCooldown(198589))
                        defSpellId = 198589; // Blur
                    else if (bot->HasSpell(196555) && !bot->GetSpellHistory()->HasCooldown(196555))
                        defSpellId = 196555; // Netherwalk
                    break;

                default:
                    break;
            }

            if (defSpellId > 0)
            {
                bb.Set<uint32>("DefensiveSpellId", defSpellId);
                return BTStatus::SUCCESS;
            }
            return BTStatus::FAILURE;
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
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;
            uint32 emergencySpellId = 0;

            switch (playerClass)
            {
                case CLASS_WARRIOR:
                    if (specId == 73) // Protection
                    {
                        // Last Stand > Shield Wall > Rallying Cry
                        if (bot->HasSpell(12975) && !bot->GetSpellHistory()->HasCooldown(12975))
                            emergencySpellId = 12975; // Last Stand
                        else if (bot->HasSpell(871) && !bot->GetSpellHistory()->HasCooldown(871))
                            emergencySpellId = 871; // Shield Wall
                        else if (bot->HasSpell(97462) && !bot->GetSpellHistory()->HasCooldown(97462))
                            emergencySpellId = 97462; // Rallying Cry
                    }
                    break;

                case CLASS_PALADIN:
                    if (specId == 66) // Protection
                    {
                        // Ardent Defender > Guardian of Ancient Kings > Divine Shield
                        if (bot->HasSpell(31850) && !bot->GetSpellHistory()->HasCooldown(31850))
                            emergencySpellId = 31850; // Ardent Defender
                        else if (bot->HasSpell(86659) && !bot->GetSpellHistory()->HasCooldown(86659))
                            emergencySpellId = 86659; // Guardian of Ancient Kings
                        else if (bot->HasSpell(642) && !bot->GetSpellHistory()->HasCooldown(642))
                            emergencySpellId = 642; // Divine Shield
                    }
                    break;

                case CLASS_DEATH_KNIGHT:
                    if (specId == 250) // Blood
                    {
                        // Vampiric Blood > Dancing Rune Weapon > Icebound Fortitude
                        if (bot->HasSpell(55233) && !bot->GetSpellHistory()->HasCooldown(55233))
                            emergencySpellId = 55233; // Vampiric Blood
                        else if (bot->HasSpell(49028) && !bot->GetSpellHistory()->HasCooldown(49028))
                            emergencySpellId = 49028; // Dancing Rune Weapon
                        else if (bot->HasSpell(48792) && !bot->GetSpellHistory()->HasCooldown(48792))
                            emergencySpellId = 48792; // Icebound Fortitude
                    }
                    break;

                case CLASS_DRUID:
                    if (specId == 104) // Guardian
                    {
                        // Survival Instincts > Barkskin > Frenzied Regeneration
                        if (bot->HasSpell(61336) && !bot->GetSpellHistory()->HasCooldown(61336))
                            emergencySpellId = 61336; // Survival Instincts
                        else if (bot->HasSpell(22812) && !bot->GetSpellHistory()->HasCooldown(22812))
                            emergencySpellId = 22812; // Barkskin
                        else if (bot->HasSpell(22842) && !bot->GetSpellHistory()->HasCooldown(22842) &&
                                 bot->GetPower(POWER_RAGE) >= 10)
                            emergencySpellId = 22842; // Frenzied Regeneration
                    }
                    break;

                case CLASS_MONK:
                    if (specId == 268) // Brewmaster
                    {
                        // Fortifying Brew > Zen Meditation > Dampen Harm
                        if (bot->HasSpell(115203) && !bot->GetSpellHistory()->HasCooldown(115203))
                            emergencySpellId = 115203; // Fortifying Brew
                        else if (bot->HasSpell(115176) && !bot->GetSpellHistory()->HasCooldown(115176))
                            emergencySpellId = 115176; // Zen Meditation
                        else if (bot->HasSpell(122278) && !bot->GetSpellHistory()->HasCooldown(122278))
                            emergencySpellId = 122278; // Dampen Harm
                    }
                    break;

                case CLASS_DEMON_HUNTER:
                    if (specId == 581) // Vengeance
                    {
                        // Metamorphosis > Fiery Brand > Demon Spikes
                        if (bot->HasSpell(187827) && !bot->GetSpellHistory()->HasCooldown(187827))
                            emergencySpellId = 187827; // Metamorphosis
                        else if (bot->HasSpell(204021) && !bot->GetSpellHistory()->HasCooldown(204021))
                            emergencySpellId = 204021; // Fiery Brand
                        else if (bot->HasSpell(203720) && !bot->GetSpellHistory()->HasCooldown(203720))
                            emergencySpellId = 203720; // Demon Spikes
                    }
                    break;

                default:
                    break;
            }

            if (emergencySpellId > 0)
            {
                bb.Set<uint32>("EmergencySpellId", emergencySpellId);
                return BTStatus::SUCCESS;
            }
            return BTStatus::FAILURE;
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
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;
            uint32 threatSpellId = 0;

            switch (playerClass)
            {
                case CLASS_WARRIOR:
                    if (specId == 73) // Protection
                    {
                        // Priority: Shield Slam > Revenge > Thunder Clap
                        if (bot->HasSpell(23922) && !bot->GetSpellHistory()->HasCooldown(23922))
                            threatSpellId = 23922; // Shield Slam
                        else if (bot->HasSpell(6572) && !bot->GetSpellHistory()->HasCooldown(6572))
                            threatSpellId = 6572; // Revenge
                        else if (bot->HasSpell(6343) && !bot->GetSpellHistory()->HasCooldown(6343))
                            threatSpellId = 6343; // Thunder Clap
                    }
                    break;

                case CLASS_PALADIN:
                    if (specId == 66) // Protection
                    {
                        // Priority: Avenger's Shield > Judgment > Consecration
                        if (bot->HasSpell(31935) && !bot->GetSpellHistory()->HasCooldown(31935))
                            threatSpellId = 31935; // Avenger's Shield
                        else if (bot->HasSpell(275779) && !bot->GetSpellHistory()->HasCooldown(275779))
                            threatSpellId = 275779; // Judgment
                        else if (bot->HasSpell(26573) && !bot->GetSpellHistory()->HasCooldown(26573))
                            threatSpellId = 26573; // Consecration
                    }
                    break;

                case CLASS_DEATH_KNIGHT:
                    if (specId == 250) // Blood
                    {
                        // Priority: Heart Strike > Blood Boil > Death and Decay
                        if (bot->HasSpell(206930) && !bot->GetSpellHistory()->HasCooldown(206930) &&
                            bot->GetPower(POWER_RUNES) >= 1)
                            threatSpellId = 206930; // Heart Strike
                        else if (bot->HasSpell(50842) && !bot->GetSpellHistory()->HasCooldown(50842))
                            threatSpellId = 50842; // Blood Boil
                        else if (bot->HasSpell(43265) && !bot->GetSpellHistory()->HasCooldown(43265) &&
                                 bot->GetPower(POWER_RUNES) >= 1)
                            threatSpellId = 43265; // Death and Decay
                    }
                    break;

                case CLASS_DRUID:
                    if (specId == 104) // Guardian
                    {
                        // Priority: Mangle > Thrash > Swipe
                        if (bot->HasSpell(33917) && !bot->GetSpellHistory()->HasCooldown(33917))
                            threatSpellId = 33917; // Mangle
                        else if (bot->HasSpell(77758) && !bot->GetSpellHistory()->HasCooldown(77758))
                            threatSpellId = 77758; // Thrash
                        else if (bot->HasSpell(213764) && !bot->GetSpellHistory()->HasCooldown(213764))
                            threatSpellId = 213764; // Swipe
                    }
                    break;

                case CLASS_MONK:
                    if (specId == 268) // Brewmaster
                    {
                        // Priority: Keg Smash > Breath of Fire > Tiger Palm
                        if (bot->HasSpell(121253) && !bot->GetSpellHistory()->HasCooldown(121253) &&
                            bot->GetPower(POWER_ENERGY) >= 40)
                            threatSpellId = 121253; // Keg Smash
                        else if (bot->HasSpell(115181) && !bot->GetSpellHistory()->HasCooldown(115181))
                            threatSpellId = 115181; // Breath of Fire
                        else if (bot->HasSpell(100780) && bot->GetPower(POWER_ENERGY) >= 25)
                            threatSpellId = 100780; // Tiger Palm
                    }
                    break;

                case CLASS_DEMON_HUNTER:
                    if (specId == 581) // Vengeance
                    {
                        // Priority: Fracture > Immolation Aura > Shear
                        if (bot->HasSpell(263642) && !bot->GetSpellHistory()->HasCooldown(263642))
                            threatSpellId = 263642; // Fracture
                        else if (bot->HasSpell(258920) && !bot->GetSpellHistory()->HasCooldown(258920))
                            threatSpellId = 258920; // Immolation Aura
                        else if (bot->HasSpell(203782))
                            threatSpellId = 203782; // Shear
                    }
                    break;

                default:
                    break;
            }

            if (threatSpellId > 0)
            {
                bb.Set<uint32>("ThreatSpellId", threatSpellId);
                return BTStatus::SUCCESS;
            }
            return BTStatus::FAILURE;
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
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;

            // Pure melee classes
            switch (playerClass)
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                case CLASS_DEATH_KNIGHT:
                    return true;

                case CLASS_PALADIN:
                    // Retribution and Protection are melee
                    return specId == 70 || specId == 66; // Ret or Prot

                case CLASS_HUNTER:
                    // Survival (melee spec)
                    return specId == 255;

                case CLASS_SHAMAN:
                    // Enhancement is melee
                    return specId == 263;

                case CLASS_MONK:
                    // Windwalker and Brewmaster are melee
                    return specId == 269 || specId == 268;

                case CLASS_DRUID:
                    // Feral and Guardian are melee
                    return specId == 103 || specId == 104;

                case CLASS_DEMON_HUNTER:
                    return true; // Both specs are melee

                default:
                    return false;
            }
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
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;

            // Pure ranged classes
            switch (playerClass)
            {
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                    return true;

                case CLASS_HUNTER:
                    // Beast Mastery and Marksmanship are ranged
                    return specId == 253 || specId == 254;

                case CLASS_PRIEST:
                    // Shadow and Discipline (when DPSing) are ranged
                    return specId == 258 || specId == 256;

                case CLASS_SHAMAN:
                    // Elemental and Restoration are ranged
                    return specId == 262 || specId == 264;

                case CLASS_DRUID:
                    // Balance and Restoration are ranged
                    return specId == 102 || specId == 105;

                case CLASS_MONK:
                    // Mistweaver is ranged
                    return specId == 270;

                case CLASS_PALADIN:
                    // Holy is ranged
                    return specId == 65;

                case CLASS_EVOKER:
                    return true; // All specs are ranged

                default:
                    return false;
            }
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
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot || !bot->IsInCombat()) return false;

            // Get combat start time from blackboard
            uint32 combatStartTime = 0;
            if (!bb.Get<uint32>("CombatStartTime", combatStartTime))
            {
                // Store current time as combat start
                combatStartTime = GameTime::GetGameTimeMS();
                bb.Set<uint32>("CombatStartTime", combatStartTime);
            }

            // Check if we're within first 5 seconds of combat
            uint32 currentTime = GameTime::GetGameTimeMS();
            uint32 combatDuration = currentTime - combatStartTime;
            return combatDuration < 5000; // 5 seconds
        }
    ));
    root->AddChild(canBuffSelector);

    // Check missing self buff
    root->AddChild(::std::make_shared<BTCondition>("MissingBuff",
        [](BotAI* ai, BTBlackboard& bb) {
            if (!ai) return false;
            Player* bot = ai->GetBot();
            if (!bot) return false;

            uint8 playerClass = bot->GetClass();
            ChrSpecializationEntry const* spec = bot->GetPrimarySpecializationEntry();
            uint32 specId = spec ? spec->ID : 0;
            uint32 missingBuffSpell = 0;

            switch (playerClass)
            {
                case CLASS_WARRIOR:
                    // Battle Shout (6673)
                    if (bot->HasSpell(6673) && !bot->HasAura(6673))
                        missingBuffSpell = 6673;
                    break;

                case CLASS_MAGE:
                    // Arcane Intellect (1459)
                    if (bot->HasSpell(1459) && !bot->HasAura(1459))
                        missingBuffSpell = 1459;
                    break;

                case CLASS_PRIEST:
                    // Power Word: Fortitude (21562)
                    if (bot->HasSpell(21562) && !bot->HasAura(21562))
                        missingBuffSpell = 21562;
                    break;

                case CLASS_PALADIN:
                    // Devotion Aura or other auras
                    if (bot->HasSpell(465) && !bot->HasAura(465))
                        missingBuffSpell = 465;
                    break;

                case CLASS_DRUID:
                    // Mark of the Wild (1126)
                    if (bot->HasSpell(1126) && !bot->HasAura(1126))
                        missingBuffSpell = 1126;
                    break;

                case CLASS_SHAMAN:
                    // Lightning Shield (192106) or Water Shield based on spec
                    if (specId == 262 || specId == 263) // Elemental/Enhancement
                    {
                        if (bot->HasSpell(192106) && !bot->HasAura(192106))
                            missingBuffSpell = 192106;
                    }
                    break;

                case CLASS_MONK:
                    // Legacy of the Emperor/Mystic Touch
                    if (bot->HasSpell(115921) && !bot->HasAura(115921))
                        missingBuffSpell = 115921;
                    break;

                case CLASS_EVOKER:
                    // Blessing of the Bronze (381748)
                    if (bot->HasSpell(381748) && !bot->HasAura(381748))
                        missingBuffSpell = 381748;
                    break;

                default:
                    break;
            }

            if (missingBuffSpell > 0)
            {
                bb.Set<uint32>("MissingBuffSpellId", missingBuffSpell);
                return true;
            }
            return false;
        }
    ));

    // Cast self buff
    root->AddChild(::std::make_shared<BTAction>("CastBuff",
        [](BotAI* ai, BTBlackboard& bb) -> BTStatus {
            if (!ai) return BTStatus::INVALID;
            Player* bot = ai->GetBot();
            if (!bot) return BTStatus::INVALID;

            uint32 buffSpellId = 0;
            if (!bb.Get<uint32>("MissingBuffSpellId", buffSpellId) || buffSpellId == 0)
                return BTStatus::FAILURE;

            // Check if spell is ready
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(buffSpellId, DIFFICULTY_NONE);
            if (!spellInfo)
                return BTStatus::FAILURE;

            if (!bot->HasSpell(buffSpellId))
                return BTStatus::FAILURE;

            if (bot->GetSpellHistory()->HasCooldown(buffSpellId))
                return BTStatus::FAILURE;

            // Check mana requirements
            ::std::vector<SpellPowerCost> costs = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
            for (const SpellPowerCost& cost : costs)
            {
                if (cost.Power == POWER_MANA && bot->GetPower(POWER_MANA) < cost.Amount)
                    return BTStatus::FAILURE;
            }

            // Store spell to cast
            bb.Set<uint32>("SpellToCast", buffSpellId);
            bb.Set<ObjectGuid>("SpellTarget", bot->GetGUID()); // Self-cast

            // Signal success - actual casting handled by spell system
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
