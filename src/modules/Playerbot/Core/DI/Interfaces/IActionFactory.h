/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "Define.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Playerbot
{

// Forward declarations
class Action;

/**
 * @brief Interface for action factory
 *
 * Provides action registration and creation functionality for the bot AI system.
 */
class TC_GAME_API IActionFactory
{
public:
    virtual ~IActionFactory() = default;

    // Action registration
    virtual void RegisterAction(std::string const& name,
                               std::function<std::shared_ptr<Action>()> creator) = 0;

    // Action creation
    virtual std::shared_ptr<Action> CreateAction(std::string const& name) = 0;
    virtual std::vector<std::shared_ptr<Action>> CreateClassActions(uint8 classId, uint8 spec) = 0;
    virtual std::vector<std::shared_ptr<Action>> CreateCombatActions(uint8 classId) = 0;
    virtual std::vector<std::shared_ptr<Action>> CreateMovementActions() = 0;

    // Available actions
    virtual std::vector<std::string> GetAvailableActions() const = 0;
    virtual bool HasAction(std::string const& name) const = 0;
};

} // namespace Playerbot
