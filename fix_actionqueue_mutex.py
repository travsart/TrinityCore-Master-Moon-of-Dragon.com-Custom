#!/usr/bin/env python3
"""
Add mutex protection to all _actionQueue accesses in CombatBehaviorIntegration.cpp
"""

import re

def add_mutex_protection():
    file_path = r"C:\TrinityBots\TrinityCore\src\modules\Playerbot\AI\Combat\CombatBehaviorIntegration.cpp"

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content

    # Fix 1: UpdatePriorities() - wrap entire function body
    content = re.sub(
        r'(void CombatBehaviorIntegration::UpdatePriorities\(\)\n\{)\n(    const CombatMetrics&)',
        r'\1\n    std::lock_guard<std::mutex> lock(_actionQueueMutex);\n    \2',
        content
    )

    # Fix 2: PrioritizeActions() - wrap entire function body (has sort and resize)
    content = re.sub(
        r'(void CombatBehaviorIntegration::PrioritizeActions\(\)\n\{)\n(    // Sort)',
        r'\1\n    std::lock_guard<std::mutex> lock(_actionQueueMutex);\n    \2',
        content
    )

    # Fix 3: GetNextAction() - wrap entire function body
    content = re.sub(
        r'(RecommendedAction CombatBehaviorIntegration::GetNextAction\(\)\n\{)\n(    if \(_actionQueue\.empty\(\)\))',
        r'\1\n    std::lock_guard<std::mutex> lock(_actionQueueMutex);\n    \2',
        content
    )

    # Fix 4: HasPendingAction() const - wrap body
    content = re.sub(
        r'(bool CombatBehaviorIntegration::HasPendingAction\(\) const\n\{)\n(    return !_actionQueue\.empty\(\);)',
        r'\1\n    std::lock_guard<std::mutex> lock(_actionQueueMutex);\n    \2',
        content
    )

    # Fix 5: ClearPendingActions() - wrap body
    content = re.sub(
        r'(void CombatBehaviorIntegration::ClearPendingActions\(\)\n\{)\n(    _actionQueue\.clear\(\);)',
        r'\1\n    std::lock_guard<std::mutex> lock(_actionQueueMutex);\n    \2',
        content
    )

    # Fix 6: DumpState() const - protect _actionQueue.size() call
    content = re.sub(
        r'(    TC_LOG_INFO\("bot\.playerbot", "Pending Actions: \{\}", )(_actionQueue\.size\(\))\);',
        r'{ std::lock_guard<std::mutex> lock(_actionQueueMutex); \1\2); }',
        content
    )

    # Fix 7: Reset() - protect clear() call
    content = re.sub(
        r'(void CombatBehaviorIntegration::Reset\(\)\n\{(?:.*?\n)*?)(    _actionQueue\.clear\(\);)',
        lambda m: m.group(1) + '    { std::lock_guard<std::mutex> lock(_actionQueueMutex); _actionQueue.clear(); }',
        content,
        flags=re.DOTALL
    )

    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("✅ Added mutex protection to _actionQueue accesses")
        print("Modified functions:")
        print("  - UpdatePriorities()")
        print("  - PrioritizeActions()")
        print("  - GetNextAction()")
        print("  - HasPendingAction()")
        print("  - ClearPendingActions()")
        print("  - DumpState()")
        print("  - Reset()")
        return True
    else:
        print("⚠️  No changes made - patterns not found")
        return False

if __name__ == "__main__":
    add_mutex_protection()
