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

#include "BotMovementConfig.h"
#include "PlayerbotConfig.h"
#include "Config.h"

void BotMovementConfig::Load()
{
    // Main enable/disable toggle
    _enabled = sPlayerbotConfig->GetBool("BotMovement.Enable", true);

    // Validation level
    uint32 validationLevelValue = sPlayerbotConfig->GetInt("BotMovement.ValidationLevel", 2);
    if (validationLevelValue > static_cast<uint32>(ValidationLevel::Strict))
        validationLevelValue = static_cast<uint32>(ValidationLevel::Standard);
    _validationLevel = static_cast<ValidationLevel>(validationLevelValue);

    // Individual validation toggles
    _groundValidation = sPlayerbotConfig->GetBool("BotMovement.Validation.Ground", true);
    _collisionValidation = sPlayerbotConfig->GetBool("BotMovement.Validation.Collision", true);
    _liquidValidation = sPlayerbotConfig->GetBool("BotMovement.Validation.Liquid", true);

    // Stuck detection settings
    _stuckDetectionEnabled = sPlayerbotConfig->GetBool("BotMovement.StuckDetection.Enable", true);
    _stuckPosThreshold = Milliseconds(sPlayerbotConfig->GetInt("BotMovement.StuckDetection.PositionThreshold", 3000));
    _stuckDistThreshold = sPlayerbotConfig->GetFloat("BotMovement.StuckDetection.Threshold", 2.0f);
    _maxRecoveryAttempts = sPlayerbotConfig->GetInt("BotMovement.StuckDetection.RecoveryMaxAttempts", 5);

    // Path cache settings
    _pathCacheEnabled = sPlayerbotConfig->GetBool("BotMovement.PathCache.Enable", true);
    _pathCacheSize = sPlayerbotConfig->GetInt("BotMovement.PathCache.MaxSize", 5000);
    _pathCacheTTL = std::chrono::duration_cast<Seconds>(Milliseconds(sPlayerbotConfig->GetInt("BotMovement.PathCache.TTL", 30000)));

    // Debug settings
    _debugLogLevel = sPlayerbotConfig->GetInt("BotMovement.Debug.LogLevel", 2);
    _logStateChanges = sPlayerbotConfig->GetBool("BotMovement.Debug.LogStateChanges", false);
    _logValidationFailures = sPlayerbotConfig->GetBool("BotMovement.Debug.LogValidationFailures", true);
}

void BotMovementConfig::Reload()
{
    Load();
}
