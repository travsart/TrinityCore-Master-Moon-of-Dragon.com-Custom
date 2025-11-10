/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 */

#pragma once

#include "Define.h"
#include <cstdint>
#include <string>

namespace Playerbot
{

/**
 * @brief Interface for Bot Behavior Management
 *
 * Abstracts bot behavior management to enable dependency injection and testing.
 * Base interface for all manager classes that control bot behavior.
 *
 * **Responsibilities:**
 * - Update behavior logic at configured intervals
 * - Enable/disable behavior execution
 * - Provide state information
 *
 * **Testability:**
 * - Can be mocked for testing AI logic without real behaviors
 * - Enables isolated testing of individual behaviors
 */
class TC_GAME_API IBehaviorManager
{
public:
    virtual ~IBehaviorManager() = default;

    /**
     * @brief Update behavior logic
     *
     * Called each frame to perform behavior updates.
     * Implementation should throttle actual work based on update interval.
     *
     * @param diff Time elapsed since last update (milliseconds)
     */
    virtual void Update(uint32 diff) = 0;

    /**
     * @brief Check if manager is enabled
     *
     * @return True if manager is processing updates, false otherwise
     */
    virtual bool IsEnabled() const = 0;

    /**
     * @brief Enable or disable manager
     *
     * @param enable True to enable updates, false to disable
     */
    virtual void SetEnabled(bool enable) = 0;

    /**
     * @brief Check if manager is currently busy
     *
     * @return True if OnUpdate() is currently executing, false otherwise
     */
    virtual bool IsBusy() const = 0;

    /**
     * @brief Get configured update interval
     *
     * @return Update interval in milliseconds
     */
    virtual uint32 GetUpdateInterval() const = 0;

    /**
     * @brief Set new update interval
     *
     * @param interval New interval in milliseconds
     */
    virtual void SetUpdateInterval(uint32 interval) = 0;

    /**
     * @brief Get manager name for debugging
     *
     * @return Manager name string
     */
    virtual std::string GetManagerName() const = 0;

    /**
     * @brief Check if manager has been initialized
     *
     * @return True if ready for updates, false otherwise
     */
    virtual bool IsInitialized() const = 0;
};

} // namespace Playerbot
