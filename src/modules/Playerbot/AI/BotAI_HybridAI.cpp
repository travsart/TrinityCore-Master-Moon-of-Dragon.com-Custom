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

#include "BotAI.h"
#include "HybridAIController.h"
#include "Log.h"

namespace Playerbot
{

// ============================================================================
// HYBRID AI INITIALIZATION
// ============================================================================

void BotAI::InitializeHybridAI()
{
    TC_LOG_DEBUG("playerbot.ai", "Bot {} - Initializing Hybrid AI Decision System (Utility AI + Behavior Trees)",
        _bot->GetName());

    // Create Hybrid AI Controller with SharedBlackboard (Phase 4)
    _hybridAI = ::std::make_unique<HybridAIController>(this, _sharedBlackboard);

    // Initialize with default behaviors and mappings
    _hybridAI->Initialize();

    TC_LOG_INFO("playerbot.ai", "Bot {} - Hybrid AI initialized successfully with SharedBlackboard integration", _bot->GetName());
}

// ============================================================================
// HYBRID AI UPDATE
// ============================================================================

void BotAI::UpdateHybridAI(uint32 diff)
{

    // Update Hybrid AI (Utility AI decision + Behavior Tree execution)
    bool success = _hybridAI->Update(diff);

    if (!success)
    {
        TC_LOG_TRACE("playerbot.ai", "Bot {} - Hybrid AI update returned false", _bot->GetName());
    }

    // Log behavior transitions
    if (_hybridAI->BehaviorChangedThisFrame())
    {
        TC_LOG_DEBUG("playerbot.ai", "Bot {} switched to behavior: {} (tree status: {})",
            _bot->GetName(),
            _hybridAI->GetCurrentBehaviorName(),
            uint32(_hybridAI->GetCurrentTreeStatus()));
    }
}

} // namespace Playerbot
