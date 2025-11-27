#!/usr/bin/env python3
"""Fix BattlePetManager compilation errors:
1. Add missing PetMetrics fields to IBattlePetManager.h
2. Fix MovePath API call in BattlePetManager.cpp
"""

def fix_interface():
    """Add missing PetMetrics fields"""
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/Core/DI/Interfaces/IBattlePetManager.h'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Replace the PetMetrics struct with complete fields
    old_metrics = '''/**
 * @brief Battle pet metrics (moved to namespace scope to avoid conflicts)
 */
struct PetMetrics
{
    std::atomic<uint32> petsCollected{0};
    std::atomic<uint32> battlesWon{0};
    std::atomic<uint32> battlesLost{0};
    std::atomic<uint32> raresCaptured{0};
    std::atomic<uint32> petsLeveled{0};
    std::atomic<uint32> totalXPGained{0};

    void Reset()
    {
        petsCollected = 0;
        battlesWon = 0;
        battlesLost = 0;
        raresCaptured = 0;
        petsLeveled = 0;
        totalXPGained = 0;
    }

    float GetWinRate() const
    {
        uint32 total = battlesWon + battlesLost;
        return total > 0 ? (float)battlesWon / total : 0.0f;
    }
};'''

    new_metrics = '''/**
 * @brief Battle pet metrics (moved to namespace scope to avoid conflicts)
 */
struct PetMetrics
{
    std::atomic<uint32> petsCollected{0};
    std::atomic<uint32> battlesWon{0};
    std::atomic<uint32> battlesLost{0};
    std::atomic<uint32> raresCaptured{0};
    std::atomic<uint32> petsLeveled{0};
    std::atomic<uint32> totalXPGained{0};

    // Additional battle statistics
    std::atomic<uint32> battlesStarted{0};     // Total battles started
    std::atomic<uint32> battlesForfeited{0};   // Battles forfeited
    std::atomic<uint32> petsSwitched{0};       // Times pet was switched during battle
    std::atomic<uint32> abilitiesUsed{0};      // Total abilities used in battles
    std::atomic<uint32> damageDealt{0};        // Total damage dealt in battles
    std::atomic<uint32> healingDone{0};        // Total healing done in battles
    std::atomic<uint32> raresFound{0};         // Rare pets discovered (not captured)
    std::atomic<uint32> criticalHits{0};       // Critical hits landed
    std::atomic<uint32> dodges{0};             // Attacks dodged

    void Reset()
    {
        petsCollected = 0;
        battlesWon = 0;
        battlesLost = 0;
        raresCaptured = 0;
        petsLeveled = 0;
        totalXPGained = 0;
        battlesStarted = 0;
        battlesForfeited = 0;
        petsSwitched = 0;
        abilitiesUsed = 0;
        damageDealt = 0;
        healingDone = 0;
        raresFound = 0;
        criticalHits = 0;
        dodges = 0;
    }

    float GetWinRate() const
    {
        uint32 total = battlesWon + battlesLost;
        return total > 0 ? (float)battlesWon / total : 0.0f;
    }

    float GetForfeitRate() const
    {
        uint32 total = battlesStarted.load();
        return total > 0 ? (float)battlesForfeited / total : 0.0f;
    }

    float GetAverageAbilitiesPerBattle() const
    {
        uint32 total = battlesStarted.load();
        return total > 0 ? (float)abilitiesUsed / total : 0.0f;
    }

    float GetDamagePerBattle() const
    {
        uint32 total = battlesWon + battlesLost;
        return total > 0 ? (float)damageDealt / total : 0.0f;
    }
};'''

    content = content.replace(old_metrics, new_metrics)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Successfully updated {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False


def fix_movepath():
    """Fix MovePath API call - use MovePoint instead"""
    filepath = 'C:/TrinityBots/TrinityCore/src/modules/Playerbot/Companion/BattlePetManager.cpp'

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Replace the MovePath call with MovePoint for direct navigation
    old_code = '''    // Get the path points
    Movement::PointsArray const& pathPoints = path.GetPath();
    if (pathPoints.empty())
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: Empty path to rare pet {}", speciesId);
        return false;
    }

    // Start moving along the path
    _bot->GetMotionMaster()->MovePath(path);'''

    new_code = '''    // Get the path points
    Movement::PointsArray const& pathPoints = path.GetPath();
    if (pathPoints.empty())
    {
        TC_LOG_DEBUG("playerbot", "BattlePetManager: Empty path to rare pet {}", speciesId);
        return false;
    }

    // Navigate to the target position using MovePoint
    // We move to the destination directly rather than following waypoints
    G3D::Vector3 const& dest = pathPoints.back();
    _bot->GetMotionMaster()->MovePoint(0, dest.x, dest.y, dest.z);'''

    content = content.replace(old_code, new_code)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Successfully updated {filepath}")
        return True
    else:
        print(f"No changes needed for {filepath}")
        return False


if __name__ == '__main__':
    fix_interface()
    fix_movepath()
    print("Done!")
