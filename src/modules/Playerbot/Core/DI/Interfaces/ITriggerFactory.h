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
class Trigger;

/**
 * @brief Interface for trigger factory
 *
 * Provides trigger registration and creation functionality for the bot AI system.
 */
class TC_GAME_API ITriggerFactory
{
public:
    virtual ~ITriggerFactory() = default;

    // Trigger registration
    virtual void RegisterTrigger(std::string const& name,
                                std::function<std::shared_ptr<Trigger>()> creator) = 0;

    // Trigger creation
    virtual std::shared_ptr<Trigger> CreateTrigger(std::string const& name) = 0;
    virtual std::vector<std::shared_ptr<Trigger>> CreateDefaultTriggers() = 0;
    virtual std::vector<std::shared_ptr<Trigger>> CreateCombatTriggers() = 0;
    virtual std::vector<std::shared_ptr<Trigger>> CreateQuestTriggers() = 0;

    // Available triggers
    virtual std::vector<std::string> GetAvailableTriggers() const = 0;
    virtual bool HasTrigger(std::string const& name) const = 0;
};

} // namespace Playerbot
