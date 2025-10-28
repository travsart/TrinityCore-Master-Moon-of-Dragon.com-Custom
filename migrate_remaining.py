#!/usr/bin/env python3
"""
Phase 2 Manual Cleanup - Automated migration of remaining ObjectAccessor calls
Handles multiple patterns not covered by initial script
"""

import re
import os
from pathlib import Path

class RemainingMigrator:
    def __init__(self, base_path):
        self.base_path = Path(base_path)
        self.stats = {
            'files_processed': 0,
            'calls_removed': 0,
            'lines_changed': 0
        }

    def migrate_file(self, file_path):
        """Migrate a single file"""
        print(f"Processing: {file_path}")

        with open(file_path, 'r', encoding='utf-8') as f:
            original = f.read()

        content = original
        initial_lines = len(content.splitlines())

        # Pattern 1: Simple if-check with ObjectAccessor (most common remaining pattern)
        # if (::Unit* target = ObjectAccessor::GetUnit(*ai->GetBot(), targetGuid))
        # if (Unit* target = ObjectAccessor::GetUnit(*_bot, guid))
        content = self._migrate_simple_if_pattern(content, file_path)

        # Pattern 2: Assignment followed by if-check
        # Unit* target = ObjectAccessor::GetUnit(*_bot, guid);
        # if (target) { ... }
        content = self._migrate_assignment_check_pattern(content, file_path)

        # Pattern 3: World boss check pattern (HunterAI specific)
        # ObjectAccessor::GetCreature(*_bot, _currentTarget) &&
        # ObjectAccessor::GetCreature(*_bot, _currentTarget)->isWorldBoss()
        content = self._migrate_worldboss_pattern(content, file_path)

        # Pattern 4: Chain pattern (ThreatCoordinator)
        # ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(guid), targetGuid)
        content = self._migrate_chain_pattern(content, file_path)

        # Pattern 5: DynamicObject pattern
        # DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(...)
        content = self._migrate_dynamic_object_pattern(content, file_path)

        if content != original:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)

            final_lines = len(content.splitlines())
            lines_changed = abs(final_lines - initial_lines)
            self.stats['files_processed'] += 1
            self.stats['lines_changed'] += lines_changed
            print(f"[OK] Modified {file_path.name} ({lines_changed} line changes)")
            return True
        else:
            print(f"[SKIP] No changes needed for {file_path.name}")
            return False

    def _migrate_simple_if_pattern(self, content, file_path):
        """
        Pattern: if (Unit* target = ObjectAccessor::GetUnit(*bot, guid))
        Solution: Use spatial grid snapshot instead
        """
        pattern = r'if\s*\(\s*(?:::)?Unit\*\s+(\w+)\s*=\s*ObjectAccessor::GetUnit\([^,]+,\s*(\w+)\)\s*\)'

        matches = list(re.finditer(pattern, content))
        if not matches:
            return content

        # Process matches in reverse to maintain positions
        for match in reversed(matches):
            var_name = match.group(1)
            guid_var = match.group(2)

            # Replace with spatial grid query
            replacement = f'''auto {var_name}Snapshot = SpatialGridQueryHelpers::FindUnitByGuid(_bot, {guid_var});
            if ({var_name}Snapshot && {var_name}Snapshot->isAlive)'''

            content = content[:match.start()] + replacement + content[match.end():]
            self.stats['calls_removed'] += 1

        return content

    def _migrate_assignment_check_pattern(self, content, file_path):
        """
        Pattern:
        Unit* target = ObjectAccessor::GetUnit(*_bot, guid);
        if (target) { ... }

        Solution: Use spatial grid snapshot
        """
        # Match assignment + if-check within reasonable distance
        pattern = r'([ \t]*)((?:::)?Unit\*\s+(\w+)\s*=\s*ObjectAccessor::Get(?:Unit|Creature)\([^;]+\);)\s*\n\s*if\s*\(\s*\3\s*\)'

        matches = list(re.finditer(pattern, content, re.MULTILINE))
        if not matches:
            return content

        for match in reversed(matches):
            indent = match.group(1)
            var_name = match.group(3)

            # Extract guid from original call
            guid_match = re.search(r'ObjectAccessor::Get(?:Unit|Creature)\([^,]+,\s*(\w+)\)', match.group(2))
            if not guid_match:
                continue

            guid_var = guid_match.group(1)

            # Replace with spatial grid query
            replacement = f'''{indent}auto {var_name}Snapshot = SpatialGridQueryHelpers::FindUnitByGuid(_bot, {guid_var});
{indent}if ({var_name}Snapshot && {var_name}Snapshot->isAlive)'''

            # Remove the ObjectAccessor line, keep the if-check
            start_pos = match.start()
            end_pos = content.find('if', match.end() - 20)  # Find the 'if' part

            content = content[:start_pos] + replacement + content[end_pos + 2:]
            self.stats['calls_removed'] += 1

        return content

    def _migrate_worldboss_pattern(self, content, file_path):
        """
        Pattern: ObjectAccessor::GetCreature(*_bot, _currentTarget)->isWorldBoss()
        Solution: Use spatial grid snapshot
        """
        if 'HunterAI.cpp' not in str(file_path):
            return content

        pattern = r'ObjectAccessor::GetCreature\(\*_bot,\s*_currentTarget\)\s*&&\s*\n\s*ObjectAccessor::GetCreature\(\*_bot,\s*_currentTarget\)->isWorldBoss\(\)'

        replacement = '''auto targetSnapshot = SpatialGridQueryHelpers::FindCreatureByGuid(_bot, _currentTarget);
           targetSnapshot && targetSnapshot->isWorldBoss'''

        if re.search(pattern, content):
            content = re.sub(pattern, replacement, content)
            self.stats['calls_removed'] += 2

        return content

    def _migrate_chain_pattern(self, content, file_path):
        """
        Pattern: ObjectAccessor::GetUnit(*ObjectAccessor::FindPlayer(guid), targetGuid)
        Solution: Use BotActionQueue pattern (queue action instead)
        """
        if 'ThreatCoordinator.cpp' not in str(file_path):
            return content

        # These need manual review - they're in complex threat coordination code
        # For now, comment them with migration note
        pattern = r'ObjectAccessor::GetUnit\(\*ObjectAccessor::FindPlayer\(([^)]+)\),\s*([^)]+)\)'

        matches = list(re.finditer(pattern, content))
        for match in reversed(matches):
            # Add migration comment
            comment = '/* MIGRATION TODO: Use BotActionQueue for cross-player operations */ '
            content = content[:match.start()] + comment + match.group(0) + content[match.end():]

        return content

    def _migrate_dynamic_object_pattern(self, content, file_path):
        """
        Pattern: DynamicObject* dynObj = ObjectAccessor::GetDynamicObject(...)
        Solution: Skip dynamic objects (not in spatial grid yet)
        """
        # Add migration note for dynamic objects
        pattern = r'(DynamicObject\*\s+\w+\s*=\s*ObjectAccessor::GetDynamicObject[^;]+;)'

        matches = list(re.finditer(pattern, content))
        for match in reversed(matches):
            # Add migration comment
            comment = '/* MIGRATION TODO: DynamicObject not in spatial grid */ '
            content = content[:match.start()] + comment + match.group(0) + content[match.end():]

        return content

    def run(self):
        """Run migration on all files with remaining ObjectAccessor calls"""

        files_to_migrate = [
            'AI/Actions/Action.cpp',
            'AI/Actions/SpellInterruptAction.cpp',
            'AI/BotAI_EventHandlers.cpp',
            'AI/ClassAI/Hunters/HunterAI.cpp',
            'AI/Combat/CombatBehaviorIntegration.cpp',
            'AI/Combat/CombatStateAnalyzer.cpp',
            'AI/Combat/GroupCombatTrigger.cpp',
            'AI/Combat/InterruptAwareness.cpp',
            'AI/Combat/InterruptManager.cpp',
            'AI/Combat/KitingManager.cpp',
            'AI/Combat/LineOfSightManager.cpp',
            'AI/Combat/ObstacleAvoidanceManager.cpp',
            'AI/Combat/PositionManager.cpp',
            'AI/Combat/TargetSelector.cpp',
            'AI/Combat/ThreatCoordinator.cpp',
            'AI/CombatBehaviors/AoEDecisionManager.cpp',
            'AI/CombatBehaviors/DefensiveBehaviorManager.cpp',
        ]

        print("=" * 60)
        print("PHASE 2 MANUAL CLEANUP - REMAINING OBJECTACCESSOR CALLS")
        print("=" * 60)
        print()

        for relative_path in files_to_migrate:
            full_path = self.base_path / 'src' / 'modules' / 'Playerbot' / relative_path
            if full_path.exists():
                self.migrate_file(full_path)
            else:
                print(f"[WARN] File not found: {full_path}")

        print()
        print("=" * 60)
        print("MIGRATION COMPLETE")
        print("=" * 60)
        print(f"Files processed: {self.stats['files_processed']}")
        print(f"ObjectAccessor calls removed: {self.stats['calls_removed']}")
        print(f"Lines changed: {self.stats['lines_changed']}")
        print()

if __name__ == '__main__':
    migrator = RemainingMigrator('c:/TrinityBots/TrinityCore')
    migrator.run()
