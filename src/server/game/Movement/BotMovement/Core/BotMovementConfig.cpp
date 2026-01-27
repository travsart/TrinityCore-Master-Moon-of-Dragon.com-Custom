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
#include "Config.h"

void BotMovementConfig::Load()
{
    _enabled = sConfigMgr->GetBoolDefault("BotMovement.Enable", true);
    
    uint32 validationLevelValue = sConfigMgr->GetIntDefault("BotMovement.ValidationLevel", 2);
    if (validationLevelValue > static_cast<uint32>(ValidationLevel::Strict))
        validationLevelValue = static_cast<uint32>(ValidationLevel::Standard);
    _validationLevel = static_cast<ValidationLevel>(validationLevelValue);
    
    _stuckPosThreshold = Milliseconds(sConfigMgr->GetIntDefault("BotMovement.StuckDetection.PositionThreshold", 3000));
    _stuckDistThreshold = sConfigMgr->GetFloatDefault("BotMovement.StuckDetection.DistanceThreshold", 2.0f);
    
    _maxRecoveryAttempts = sConfigMgr->GetIntDefault("BotMovement.Recovery.MaxAttempts", 5);
    
    _pathCacheSize = sConfigMgr->GetIntDefault("BotMovement.PathCache.Size", 1000);
    _pathCacheTTL = Seconds(sConfigMgr->GetIntDefault("BotMovement.PathCache.TTL", 60));
    
    _debugLogLevel = sConfigMgr->GetIntDefault("BotMovement.Debug.LogLevel", 2);
}

void BotMovementConfig::Reload()
{
    Load();
}
