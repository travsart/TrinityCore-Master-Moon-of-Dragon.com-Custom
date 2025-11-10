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
class Strategy;

/**
 * @brief Interface for strategy factory
 *
 * Provides strategy registration and creation functionality for the bot AI system.
 */
class TC_GAME_API IStrategyFactory
{
public:
    virtual ~IStrategyFactory() = default;

    // Strategy registration
    virtual void RegisterStrategy(std::string const& name,
                                 std::function<std::unique_ptr<Strategy>()> creator) = 0;

    // Strategy creation
    virtual std::unique_ptr<Strategy> CreateStrategy(std::string const& name) = 0;
    virtual std::vector<std::unique_ptr<Strategy>> CreateClassStrategies(uint8 classId, uint8 spec) = 0;
    virtual std::vector<std::unique_ptr<Strategy>> CreateLevelStrategies(uint8 level) = 0;
    virtual std::vector<std::unique_ptr<Strategy>> CreatePvPStrategies() = 0;
    virtual std::vector<std::unique_ptr<Strategy>> CreatePvEStrategies() = 0;

    // Available strategies
    virtual std::vector<std::string> GetAvailableStrategies() const = 0;
    virtual bool HasStrategy(std::string const& name) const = 0;
};

} // namespace Playerbot
