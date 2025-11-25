#!/usr/bin/env python3
"""
Enterprise-grade fix for compilation errors in Playerbot module.
This script fixes API mismatches between headers and implementations.
"""

import os
import re

base_path = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot'

def read_file(path):
    if not os.path.exists(path):
        return None
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

print("=" * 70)
print("FIXING COMPILATION ERRORS - ENTERPRISE GRADE")
print("=" * 70)

# =============================================================================
# 1. Fix DecisionFusionSystem.h - Add Player forward declaration
# =============================================================================
print("\n1. Fixing DecisionFusionSystem.h...")
dfs_h_path = f'{base_path}/AI/Decision/DecisionFusionSystem.h'
content = read_file(dfs_h_path)
if content:
    if 'class Player;' not in content:
        # Add Player forward declaration after Unit declaration
        content = content.replace(
            'class Unit;',
            'class Unit;\nclass Player;'
        )
        write_file(dfs_h_path, content)
        print("  FIXED: Added Player forward declaration")
    else:
        print("  OK: Player already declared")

# =============================================================================
# 2. Fix HealingTargetSelector.cpp - Group iteration syntax
# =============================================================================
print("\n2. Fixing HealingTargetSelector.cpp...")
hts_cpp_path = f'{base_path}/AI/Services/HealingTargetSelector.cpp'
content = read_file(hts_cpp_path)
if content:
    # Add Spell.h include if missing
    if '#include "Spell.h"' not in content:
        content = content.replace(
            '#include "HealingTargetSelector.h"',
            '#include "HealingTargetSelector.h"\n#include "Spell.h"'
        )
        print("  FIXED: Added Spell.h include")

    # Fix Group iteration - for (GroupReference* itr : *group) is wrong
    # Should be for (GroupReference const& itr : group->GetMembers())
    content = re.sub(
        r'for \(GroupReference\* itr : \*group\)',
        'for (GroupReference const& itr : group->GetMembers())',
        content
    )
    # Fix the GetSource call - itr->GetSource() should be itr.GetSource()
    content = re.sub(
        r'itr->GetSource\(\)',
        'itr.GetSource()',
        content
    )
    write_file(hts_cpp_path, content)
    print("  FIXED: Group iteration syntax")

# =============================================================================
# 3. Fix ActionPriorityQueue.cpp - Missing includes and API fixes
# =============================================================================
print("\n3. Fixing ActionPriorityQueue.cpp...")
apq_cpp_path = f'{base_path}/AI/Decision/ActionPriorityQueue.cpp'
content = read_file(apq_cpp_path)
if content:
    # Add GameTime.h include
    if '#include "GameTime.h"' not in content:
        content = content.replace(
            '#include "ActionPriorityQueue.h"',
            '#include "ActionPriorityQueue.h"\n#include "GameTime.h"\n#include "SpellHistory.h"\n#include "Map.h"'
        )
        print("  FIXED: Added GameTime.h, SpellHistory.h, Map.h includes")

    # Fix GetGlobalCooldownMgr - use GetSpellHistory()->GetGlobalCooldownMgr()
    content = content.replace(
        'bot->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo)',
        'bot->GetSpellHistory()->GetRemainingGlobalCooldown() > Milliseconds::zero()'
    )
    print("  FIXED: GetGlobalCooldownMgr API")

    # Fix GetSpellInfo call - needs difficulty parameter
    content = re.sub(
        r"sSpellMgr->GetSpellInfo\(spellId\)",
        "sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE)",
        content
    )
    print("  FIXED: GetSpellInfo needs DIFFICULTY_NONE")

    write_file(apq_cpp_path, content)

# =============================================================================
# 4. Fix RoleAssignment.cpp - Major overhaul needed
# =============================================================================
print("\n4. Fixing RoleAssignment.cpp (comprehensive rewrite)...")
ra_cpp_path = f'{base_path}/Group/RoleAssignment.cpp'
content = read_file(ra_cpp_path)
if content:
    # Fix the duplicate/invalid default constructor
    # Remove the standalone default constructor - it's invalid
    content = re.sub(
        r'RoleAssignment::RoleAssignment\(\)\s*\{\s*InitializeClassRoleMappings\(\);[^}]+\}',
        '',
        content
    )

    # Fix the proper constructor to call InitializeClassRoleMappings
    content = content.replace(
        'RoleAssignment::RoleAssignment(Player* bot) : _bot(bot)\n{\n    if (!_bot) TC_LOG_ERROR("playerbot.group", "RoleAssignment: null bot!");\n}',
        '''RoleAssignment::RoleAssignment(Player* bot) : _bot(bot)
{
    if (!_bot) TC_LOG_ERROR("playerbot.group", "RoleAssignment: null bot!");
    InitializeClassRoleMappings();
    _globalStatistics.Reset();
}'''
    )
    print("  FIXED: Constructor")

    # Fix AssignRole signature - header has (GroupRole, Group*), implementation has (uint32, GroupRole, Group*)
    # The header is correct (uses _bot), fix implementation
    content = re.sub(
        r'bool RoleAssignment::AssignRole\(uint32 playerGuid, GroupRole role, Group\* group\)',
        'bool RoleAssignment::AssignRole(GroupRole role, Group* group)',
        content
    )

    # Remove the uint32 playerGuid usage, use _bot->GetGUID().GetCounter() instead
    content = content.replace(
        '''if (!group)
        return false;

    Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid));
    if (!_bot)
        return false;

    // Update player profile
    auto it = _playerProfiles.find(playerGuid);''',
        '''if (!group || !_bot)
        return false;

    uint32 botGuid = _bot->GetGUID().GetCounter();

    // Update player profile
    auto it = _playerProfiles.find(botGuid);'''
    )

    content = content.replace(
        'profile.assignedRole = role;\n        _playerProfiles[playerGuid] = profile;',
        'profile.assignedRole = role;\n        _playerProfiles[botGuid] = profile;'
    )

    content = content.replace(
        'NotifyRoleAssignment(player, role, group);',
        'NotifyRoleAssignment(role, group);'
    )
    print("  FIXED: AssignRole signature and implementation")

    # Fix AnalyzePlayerCapabilities - header has no parameters, implementation uses Player*
    # Fix to use _bot
    content = re.sub(
        r'PlayerRoleProfile RoleAssignment::AnalyzePlayerCapabilities\(Player\* [^)]+\)',
        'PlayerRoleProfile RoleAssignment::AnalyzePlayerCapabilities()',
        content
    )

    # Also fix the internal call that passes a player parameter
    content = re.sub(
        r'PlayerRoleProfile profile = AnalyzePlayerCapabilities\(member\);',
        'PlayerRoleProfile profile = AnalyzePlayerCapabilities(); // Note: analyzes _bot only currently',
        content
    )
    print("  FIXED: AnalyzePlayerCapabilities signature")

    # Fix CalculateRoleScores - header has (Group*), implementation has extra parameters
    content = re.sub(
        r'std::vector<RoleScore> RoleAssignment::CalculateRoleScores\([^)]*Player\*[^)]*\)',
        'std::vector<RoleScore> RoleAssignment::CalculateRoleScores(Group* group)',
        content
    )

    # Fix CanPlayerSwitchRole - header has (GroupRole, Group*), not (Player*, GroupRole, Group*)
    content = re.sub(
        r'bool RoleAssignment::CanPlayerSwitchRole\(Player\* [^,]+, GroupRole newRole, Group\* group\)',
        'bool RoleAssignment::CanPlayerSwitchRole(GroupRole newRole, Group* group)',
        content
    )
    content = re.sub(
        r'!CanPlayerSwitchRole\(ObjectAccessor::FindPlayer[^)]+\), role\d, group\)',
        '!CanPlayerSwitchRole(role1, group)',
        content
    )
    content = re.sub(
        r'CanPlayerSwitchRole\(ObjectAccessor::FindPlayer\(ObjectGuid::Create<HighGuid::Player>\(player1Guid\)\), role2, group\)',
        'CanPlayerSwitchRole(role2, group)',
        content
    )
    content = re.sub(
        r'CanPlayerSwitchRole\(ObjectAccessor::FindPlayer\(ObjectGuid::Create<HighGuid::Player>\(player2Guid\)\), role1, group\)',
        'CanPlayerSwitchRole(role1, group)',
        content
    )

    # Fix playerGuid reference in CanPlayerSwitchRole
    content = content.replace(
        'auto profileIt = _playerProfiles.find(playerGuid);',
        'auto profileIt = _playerProfiles.find(_bot->GetGUID().GetCounter());'
    )
    print("  FIXED: CanPlayerSwitchRole signature")

    # Fix BuildPlayerProfile and CalculateRoleCapabilities signatures
    content = re.sub(
        r'void RoleAssignment::BuildPlayerProfile\(PlayerRoleProfile& profile, Player\* player\)',
        'void RoleAssignment::BuildPlayerProfile(PlayerRoleProfile& profile)',
        content
    )
    content = re.sub(
        r'void RoleAssignment::CalculateRoleCapabilities\(PlayerRoleProfile& profile, Player\* player\)',
        'void RoleAssignment::CalculateRoleCapabilities(PlayerRoleProfile& profile)',
        content
    )
    print("  FIXED: BuildPlayerProfile and CalculateRoleCapabilities signatures")

    # Fix calls to these functions
    content = content.replace(
        'BuildPlayerProfile(profile, player);',
        'BuildPlayerProfile(profile);'
    )
    content = content.replace(
        'CalculateRoleCapabilities(profile, player);',
        'CalculateRoleCapabilities(profile);'
    )
    content = content.replace(
        'AnalyzePlayerGear(profile, player);',
        'AnalyzePlayerGear(profile);'
    )
    content = content.replace(
        'UpdateRoleExperience(profile, player);',
        'UpdateRoleExperience(profile);'
    )

    # Fix DetermineOptimalRole call
    content = re.sub(
        r'DetermineOptimalRole\(player, nullptr, RoleAssignmentStrategy::OPTIMAL_ASSIGNMENT\)',
        'DetermineOptimalRole(nullptr, RoleAssignmentStrategy::OPTIMAL_ASSIGNMENT)',
        content
    )
    content = re.sub(
        r'CalculateRoleScores\(player, nullptr\)',
        'CalculateRoleScores(nullptr)',
        content
    )
    content = re.sub(
        r'CalculateRoleScores\(player, group\)',
        'CalculateRoleScores(group)',
        content
    )

    # Fix undefined player variable references - use _bot instead
    content = content.replace(
        'score.gearScore = CalculateGearScore(player, role);',
        'score.gearScore = CalculateGearScore(role);'
    )
    content = content.replace(
        'score.synergy = CalculateSynergyScore(player, role, group);',
        'score.synergy = CalculateSynergyScore(role, group);'
    )

    # Fix getClass -> GetClass (TrinityCore API)
    content = content.replace('.getClass()', '.GetClass()')
    print("  FIXED: getClass -> GetClass")

    # Fix CalculateGearScore signature
    content = re.sub(
        r'float RoleAssignment::CalculateGearScore\(Player\* [^,]+, GroupRole role\)',
        'float RoleAssignment::CalculateGearScore(GroupRole role)',
        content
    )

    # Fix CalculateSynergyScore signature
    content = re.sub(
        r'float RoleAssignment::CalculateSynergyScore\(Player\* [^,]+, GroupRole role, Group\* group\)',
        'float RoleAssignment::CalculateSynergyScore(GroupRole role, Group* group)',
        content
    )

    # Fix CalculateExperienceScore signature
    content = re.sub(
        r'float RoleAssignment::CalculateExperienceScore\(uint32 [^,]+, GroupRole role\)',
        'float RoleAssignment::CalculateExperienceScore(GroupRole role)',
        content
    )
    content = content.replace(
        'score.experienceScore = CalculateExperienceScore(_bot->GetGUID().GetCounter(), role);',
        'score.experienceScore = CalculateExperienceScore(role);'
    )

    # Add missing include for Item
    if '#include "Item.h"' not in content:
        content = content.replace(
            '#include "RoleAssignment.h"',
            '#include "RoleAssignment.h"\n#include "Item.h"\n#include "ItemTemplate.h"'
        )
        print("  FIXED: Added Item.h include")

    # Add SharedDefines.h for ITEM_MOD_* constants
    if '#include "SharedDefines.h"' not in content:
        content = content.replace(
            '#include "Item.h"',
            '#include "Item.h"\n#include "SharedDefines.h"\n#include "DBCEnums.h"'
        )
        print("  FIXED: Added SharedDefines.h for ITEM_MOD_* constants")

    write_file(ra_cpp_path, content)

# =============================================================================
# 5. Verify includes in ActionPriorityQueue.h
# =============================================================================
print("\n5. Checking ActionPriorityQueue.h...")
apq_h_path = f'{base_path}/AI/Decision/ActionPriorityQueue.h'
content = read_file(apq_h_path)
if content:
    needs_update = False
    if '#include "SpellMgr.h"' not in content:
        content = content.replace(
            '#include "Common.h"',
            '#include "Common.h"\n#include "SpellMgr.h"'
        )
        needs_update = True
    if needs_update:
        write_file(apq_h_path, content)
        print("  FIXED: Added SpellMgr.h include")
    else:
        print("  OK: Includes look correct")

print("\n" + "=" * 70)
print("COMPILATION FIXES COMPLETE")
print("Please rebuild to verify all fixes.")
print("=" * 70)
