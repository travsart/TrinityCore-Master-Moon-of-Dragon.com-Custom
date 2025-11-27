#!/usr/bin/env python3
"""
Fix BehaviorTreeFactory.cpp - Replace placeholder implementations with full enterprise-grade code.
Target functions:
1. IsMeleeClass - Class/spec detection for melee
2. IsRangedClass - Class/spec detection for ranged
3. UseDefensive - Class-specific defensive CDs
4. CastRangedSpell - Class-specific ranged rotations
5. UseEmergencyCD - Tank emergency defensive CDs
6. GenerateThreat - Tank threat generation abilities
7. DefensiveStance - Tank stance/presence management
8. HealSelf - Self-healing spells
9. CastAoEHeal - AoE healing spells
10. MissingBuff - Buff detection
11. CastBuff - Buff casting
12. EarlyCombat - Combat duration check
"""

import sys

# Read the file
with open('src/modules/Playerbot/AI/BehaviorTree/BehaviorTreeFactory.cpp', 'r') as f:
    content = f.read()

replacements_made = 0

# ============================================================================
# 1. IsMeleeClass - Full spec-aware melee detection
# ============================================================================

old_melee_check = '''    meleeSequence->AddChild(::std::make_shared<BTCondition>("IsMeleeClass",
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
    ));'''

new_melee_check = '''    meleeSequence->AddChild(::std::make_shared<BTCondition>("IsMeleeClass",
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
    ));'''

if old_melee_check in content:
    content = content.replace(old_melee_check, new_melee_check)
    print("IsMeleeClass: REPLACED")
    replacements_made += 1
else:
    print("IsMeleeClass: NOT FOUND")

# ============================================================================
# 2. IsRangedClass - Full spec-aware ranged detection
# ============================================================================

old_ranged_check = '''    rangedSequence->AddChild(::std::make_shared<BTCondition>("IsRangedClass",
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
    ));'''

new_ranged_check = '''    rangedSequence->AddChild(::std::make_shared<BTCondition>("IsRangedClass",
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
    ));'''

if old_ranged_check in content:
    content = content.replace(old_ranged_check, new_ranged_check)
    print("IsRangedClass: REPLACED")
    replacements_made += 1
else:
    print("IsRangedClass: NOT FOUND")

# ============================================================================
# 3. UseDefensive - Class-specific defensive cooldowns for melee
# ============================================================================

old_defensive = '''    // If low health, try to use defensive CD (placeholder - class-specific)
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
    ));'''

new_defensive = '''    // If low health, try to use defensive CD (class-specific)
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
    ));'''

if old_defensive in content:
    content = content.replace(old_defensive, new_defensive)
    print("UseDefensive: REPLACED")
    replacements_made += 1
else:
    print("UseDefensive: NOT FOUND")

# ============================================================================
# 4. EarlyCombat check - Combat duration tracking
# ============================================================================

old_early_combat = '''    canBuffSelector->AddChild(::std::make_shared<BTCondition>("EarlyCombat",
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
    ));'''

new_early_combat = '''    canBuffSelector->AddChild(::std::make_shared<BTCondition>("EarlyCombat",
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
    ));'''

if old_early_combat in content:
    content = content.replace(old_early_combat, new_early_combat)
    print("EarlyCombat: REPLACED")
    replacements_made += 1
else:
    print("EarlyCombat: NOT FOUND")

# ============================================================================
# 5. MissingBuff check - Class buff detection
# ============================================================================

old_missing_buff = '''    // Check missing self buff
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
    ));'''

new_missing_buff = '''    // Check missing self buff
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
    ));'''

if old_missing_buff in content:
    content = content.replace(old_missing_buff, new_missing_buff)
    print("MissingBuff: REPLACED")
    replacements_made += 1
else:
    print("MissingBuff: NOT FOUND")

# ============================================================================
# 6. CastBuff - Cast the detected missing buff
# ============================================================================

old_cast_buff = '''    // Cast self buff
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
    ));'''

new_cast_buff = '''    // Cast self buff
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
    ));'''

if old_cast_buff in content:
    content = content.replace(old_cast_buff, new_cast_buff)
    print("CastBuff: REPLACED")
    replacements_made += 1
else:
    print("CastBuff: NOT FOUND")

# ============================================================================
# 7. Tank UseEmergencyCD - Tank emergency defensive cooldowns
# ============================================================================

old_emergency_cd = '''    emergencyDefensive->AddChild(::std::make_shared<BTAction>("UseEmergencyCD",
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
    ));'''

new_emergency_cd = '''    emergencyDefensive->AddChild(::std::make_shared<BTAction>("UseEmergencyCD",
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
    ));'''

if old_emergency_cd in content:
    content = content.replace(old_emergency_cd, new_emergency_cd)
    print("UseEmergencyCD: REPLACED")
    replacements_made += 1
else:
    print("UseEmergencyCD: NOT FOUND")

# ============================================================================
# 8. GenerateThreat - Tank threat generation abilities
# ============================================================================

old_generate_threat = '''    // Use threat generation
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
    ));'''

new_generate_threat = '''    // Use threat generation
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
    ));'''

if old_generate_threat in content:
    content = content.replace(old_generate_threat, new_generate_threat)
    print("GenerateThreat: REPLACED")
    replacements_made += 1
else:
    print("GenerateThreat: NOT FOUND")

# ============================================================================
# Write back
# ============================================================================

with open('src/modules/Playerbot/AI/BehaviorTree/BehaviorTreeFactory.cpp', 'w') as f:
    f.write(content)

print(f"\nTotal replacements made: {replacements_made}")
print("File updated successfully" if replacements_made > 0 else "No changes made - check patterns")
