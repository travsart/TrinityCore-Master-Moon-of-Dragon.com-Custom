#!/usr/bin/env python3
"""
Lock-Free Migration Script
Automates removal of ObjectAccessor fallbacks from worker thread code
"""

import re
import sys
from pathlib import Path

class LockFreeMigrator:
    def __init__(self, base_path):
        self.base_path = Path(base_path)
        self.stats = {"files_processed": 0, "fallbacks_removed": 0, "lines_changed": 0}

    def remove_objectaccessor_fallback(self, content, file_path):
        """
        Remove ObjectAccessor::Get* fallback pattern after spatial grid query

        Pattern:
            auto snapshot = SpatialGridQueryHelpers::Find...
            if (!snapshot || !snapshot->IsAlive())
                continue;

            Unit* target = ObjectAccessor::GetUnit(*_bot, guid);  // REMOVE THIS
            if (!target)
                continue;

            DoSomething(target);  // Use snapshot instead
        """
        lines = content.split('\n')
        new_lines = []
        i = 0
        changes = 0

        while i < len(lines):
            line = lines[i]

            # Check for ObjectAccessor::Get pattern
            if 'ObjectAccessor::Get' in line and '//' not in line[:line.find('ObjectAccessor')]:
                # Look back to see if there's a snapshot check nearby
                has_snapshot_check = False
                for j in range(max(0, i-10), i):
                    if 'snapshot' in lines[j] and ('FindCreatureByGuid' in lines[j] or 'Find' in lines[j]):
                        has_snapshot_check = True
                        break

                if has_snapshot_check:
                    # This is likely a fallback - skip this line and the next null check
                    print(f"  Removing fallback at {file_path}:{i+1}: {line.strip()}")
                    changes += 1

                    # Skip the ObjectAccessor line
                    i += 1

                    # Skip the null check if present
                    while i < len(lines) and lines[i].strip().startswith('if (!'):
                        i += 1
                        # Skip the continue/return
                        if i < len(lines) and 'continue' in lines[i]:
                            i += 1
                            break

                    continue

            new_lines.append(line)
            i += 1

        if changes > 0:
            self.stats["fallbacks_removed"] += changes
            self.stats["lines_changed"] += changes * 3  # Approx 3 lines per fallback
            return '\n'.join(new_lines)

        return content

    def replace_cell_visit(self, content, file_path):
        """
        Replace Cell::VisitGridObjects with spatial grid query
        """
        if 'Cell::Visit' not in content:
            return content

        # Find function containing Cell::Visit
        pattern = r'(std::vector<[:\w*]+>\s+\w+::\w+\([^)]*\)\s+const\s*\{[^}]+Cell::Visit[^}]+\})'

        def replace_function(match):
            func = match.group(1)

            # Check if it's a GetNearby... function
            if 'GetNearby' not in func:
                return func

            # Extract function signature
            sig_match = re.search(r'(std::vector<[:\w*]+>)\s+(\w+::)?(\w+)\(([^)]*)\)', func)
            if not sig_match:
                return func

            return_type = sig_match.group(1)
            method_name = sig_match.group(3)
            params = sig_match.group(4)

            # Generate new implementation
            new_impl = f'''std::vector<ObjectGuid> {method_name}({params}) const
{{
    std::vector<ObjectGuid> guids;

    auto grid = sSpatialGridManager.GetGrid(_bot->GetMap());
    if (!grid) return guids;

    float range = {params.split('=')[1].strip() if '=' in params else params.split()[-1]};
    auto creatures = grid->QueryNearbyCreatures(_bot->GetPosition(), range);

    for (auto const& snapshot : creatures) {{
        if (snapshot.isAlive && snapshot.isHostile)
            guids.push_back(snapshot.guid);
    }}

    return guids;
}}'''

            print(f"  Replacing Cell::Visit in {method_name} at {file_path}")
            self.stats["lines_changed"] += 10
            return new_impl

        return re.sub(pattern, replace_function, content, flags=re.DOTALL)

    def process_file(self, file_path):
        """Process a single file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            original_content = content

            # Remove ObjectAccessor fallbacks
            content = self.remove_objectaccessor_fallback(content, file_path)

            # Replace Cell::Visit calls
            content = self.replace_cell_visit(content, file_path)

            # Write back if changed
            if content != original_content:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                self.stats["files_processed"] += 1
                print(f"[OK] Migrated: {file_path}")
                return True

            return False

        except Exception as e:
            print(f"[ERROR] Error processing {file_path}: {e}")
            return False

    def migrate_all(self):
        """Migrate all AI files"""
        ai_path = self.base_path / 'src' / 'modules' / 'Playerbot' / 'AI'

        if not ai_path.exists():
            print(f"Error: AI path not found: {ai_path}")
            return

        print("Starting lock-free migration...")
        print(f"Base path: {self.base_path}")
        print(f"AI path: {ai_path}\n")

        # Process all .cpp files in AI directory recursively
        cpp_files = list(ai_path.rglob('*.cpp'))
        total_files = len(cpp_files)

        print(f"Found {total_files} C++ files to process\n")

        for file_path in cpp_files:
            # Skip BotActionProcessor (it's safe to have ObjectAccessor there)
            if 'BotActionProcessor' in str(file_path):
                continue

            self.process_file(file_path)

        print(f"\n{'='*60}")
        print("Migration Complete!")
        print(f"{'='*60}")
        print(f"Files processed: {self.stats['files_processed']}")
        print(f"Fallbacks removed: {self.stats['fallbacks_removed']}")
        print(f"Lines changed: {self.stats['lines_changed']}")
        print(f"{'='*60}\n")

def main():
    if len(sys.argv) > 1:
        base_path = sys.argv[1]
    else:
        base_path = 'c:/TrinityBots/TrinityCore'

    migrator = LockFreeMigrator(base_path)
    migrator.migrate_all()

if __name__ == '__main__':
    main()
