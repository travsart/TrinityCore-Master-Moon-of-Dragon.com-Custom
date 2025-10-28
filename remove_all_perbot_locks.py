#!/usr/bin/env python3
"""
Script to remove all unnecessary mutex locks from per-bot instance managers.
This targets the 14 managers identified in Phase 2 review with 118 total locks.
"""

import re
import os

# Manager definitions with file paths and lock line numbers
MANAGERS = {
    "KitingManager": {
        "path": r"src\modules\Playerbot\AI\Combat\KitingManager.cpp",
        "locks": [53, 264],
        "comment": "No lock needed - kiting state is per-bot instance data"
    },
    "InterruptManager": {
        "path": r"src\modules\Playerbot\AI\Combat\InterruptManager.cpp",
        "locks": [57, 192],
        "comment": "No lock needed - interrupt tracking is per-bot instance data"
    },
    "ObstacleAvoidanceManager": {
        "path": r"src\modules\Playerbot\AI\Combat\ObstacleAvoidanceManager.cpp",
        "locks": [56, 108, 662],
        "comment": "No lock needed - obstacle detection is per-bot instance data"
    },
    "PositionManager": {
        "path": r"src\modules\Playerbot\AI\Combat\PositionManager.cpp",
        "locks": [59, 576],
        "comment": "No lock needed - position data is per-bot instance data"
    },
    "BotThreatManager": {
        "path": r"src\modules\Playerbot\AI\Combat\BotThreatManager.cpp",
        "locks": [74, 105, 179, 228, 256, 357, 378, 389, 473, 486, 502, 518, 531, 556, 579, 637],
        "comment": "No lock needed - threat data is per-bot instance data"
    },
    "LineOfSightManager": {
        "path": r"src\modules\Playerbot\AI\Combat\LineOfSightManager.cpp",
        "locks": [48, 447, 454],
        "comment": "No lock needed - line of sight cache is per-bot instance data"
    },
    "PathfindingManager": {
        "path": r"src\modules\Playerbot\AI\Combat\PathfindingManager.cpp",
        "locks": [47, 586, 713],
        "comment": "No lock needed - pathfinding data is per-bot instance data"
    },
    "InteractionManager": {
        "path": r"src\modules\Playerbot\Interaction\Core\InteractionManager.cpp",
        "locks": [85, 119, 157, 238, 320, 339, 348, 711, 742],
        "comment": "No lock needed - interaction state is per-bot instance data"
    },
    "InventoryManager": {
        "path": r"src\modules\Playerbot\Game\InventoryManager.cpp",
        "locks": [1296, 1313, 1353],
        "comment": "No lock needed - inventory data is per-bot instance data"
    },
    "EquipmentManager": {
        "path": r"src\modules\Playerbot\Equipment\EquipmentManager.cpp",
        "locks": [714, 1396, 1402, 1417, 1663],
        "comment": "No lock needed - equipment data is per-bot instance data"
    },
    "BotLevelManager": {
        "path": r"src\modules\Playerbot\Character\BotLevelManager.cpp",
        "locks": [98, 570, 580],
        "comment": "No lock needed - level queue is per-bot instance data"
    },
    "ProfessionManager": {
        "path": r"src\modules\Playerbot\Professions\ProfessionManager.cpp",
        "locks": [439, 844, 1009, 1023, 1126, 1188, 1194, 1209],
        "comment": "No lock needed - profession data is per-bot instance data"
    },
    "BattlePetManager": {
        "path": r"src\modules\Playerbot\Companion\BattlePetManager.cpp",
        "locks": [44, 154, 183, 202, 216, 264, 285, 348, 418, 469, 503, 536, 578, 611, 625, 659, 681, 722, 751, 777, 827, 840, 862, 885, 891, 905],
        "comment": "No lock needed - battle pet data is per-bot instance data"
    },
    "MountManager": {
        "path": r"src\modules\Playerbot\Companion\MountManager.cpp",
        "locks": [48, 217, 260, 302, 363, 394, 465, 496, 534, 736, 742, 758],
        "comment": "No lock needed - mount data is per-bot instance data"
    },
    "DeathRecoveryManager": {
        "path": r"src\modules\Playerbot\Lifecycle\DeathRecoveryManager.cpp",
        "locks": [155, 188, 210, 235, 1110, 1122, 1151, 1179, 1185, 1196],
        "comment": "No lock needed - death recovery state is per-bot instance data"
    },
    "BotPriorityManager": {
        "path": r"src\modules\Playerbot\Session\BotPriorityManager.cpp",
        "locks": [33, 78, 95, 141, 165, 328, 336, 355, 363, 371, 380, 423, 444, 466, 494, 507, 513, 555],
        "comment": "No lock needed - priority metrics are per-bot instance data"
    }
}

def remove_locks_from_file(filepath, lock_lines, comment):
    """Remove lock_guard statements from specified lines in a file."""

    base_path = r"C:\TrinityBots\TrinityCore"
    full_path = os.path.join(base_path, filepath)

    if not os.path.exists(full_path):
        print(f"[SKIP] File not found: {full_path}")
        return 0

    with open(full_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    original_content = ''.join(lines)
    modifications = []

    # Sort lock lines in reverse order to avoid line number shifts
    sorted_locks = sorted(lock_lines, reverse=True)

    for lock_line_num in sorted_locks:
        # Convert to 0-based index
        idx = lock_line_num - 1

        if idx < 0 or idx >= len(lines):
            print(f"[WARN] Line {lock_line_num} out of range in {filepath}")
            continue

        line = lines[idx]

        # Check if this line contains a lock_guard
        if 'std::lock_guard' in line or 'std::recursive_mutex' in line:
            # Get the indentation from the original line
            indent = len(line) - len(line.lstrip())
            indent_str = ' ' * indent

            # Replace with comment
            lines[idx] = f"{indent_str}// {comment}\n"
            modifications.append(f"Line {lock_line_num}")

    if modifications:
        new_content = ''.join(lines)
        if new_content != original_content:
            with open(full_path, 'w', encoding='utf-8') as f:
                f.write(new_content)
            return len(modifications)

    return 0

def main():
    """Remove all per-bot instance manager locks."""

    total_removed = 0
    managers_modified = 0

    print("=" * 80)
    print("PHASE 2: REMOVING PER-BOT INSTANCE MANAGER LOCKS")
    print("=" * 80)
    print("")

    for manager_name, config in MANAGERS.items():
        print(f"[PROCESSING] {manager_name}...")
        print(f"  File: {config['path']}")
        print(f"  Locks to remove: {len(config['locks'])}")

        removed = remove_locks_from_file(
            config['path'],
            config['locks'],
            config['comment']
        )

        if removed > 0:
            total_removed += removed
            managers_modified += 1
            print(f"  [OK] Removed {removed} locks")
        else:
            print(f"  [WARN] No locks removed (already optimized or not found)")
        print("")

    print("=" * 80)
    print(f"[COMPLETE] Phase 2 Lock Removal")
    print(f"  Managers modified: {managers_modified} of {len(MANAGERS)}")
    print(f"  Total locks removed: {total_removed}")
    print("=" * 80)

    return total_removed

if __name__ == "__main__":
    total = main()
    exit(0 if total > 0 else 1)
