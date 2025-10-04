/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_PLAYERBOT_MOVEMENT_GENERATOR_H
#define TRINITY_PLAYERBOT_MOVEMENT_GENERATOR_H

#include "MovementTypes.h"
#include "Object.h"
#include <atomic>
#include <chrono>

class Player;
class Unit;
class MotionMaster;

namespace Playerbot
{
    /**
     * @class MovementGenerator
     * @brief Abstract base class for all bot movement behaviors
     *
     * This class provides the interface and common functionality for all movement
     * generators. Each specific movement type (follow, flee, formation, etc.)
     * inherits from this class and implements the virtual methods.
     */
    class TC_GAME_API MovementGenerator
    {
    public:
        /**
         * @brief Constructor
         * @param type The type of movement generator
         * @param priority The priority level for this generator
         */
        explicit MovementGenerator(MovementGeneratorType type,
                                 MovementPriority priority = MovementPriority::PRIORITY_NORMAL);

        /**
         * @brief Virtual destructor for proper cleanup
         */
        virtual ~MovementGenerator();

        /**
         * @brief Initialize the movement generator
         * @param bot The bot player to move
         * @return True if initialization successful
         */
        virtual bool Initialize(Player* bot) = 0;

        /**
         * @brief Reset the movement generator to initial state
         * @param bot The bot player
         */
        virtual void Reset(Player* bot) = 0;

        /**
         * @brief Update the movement generator
         * @param bot The bot player
         * @param diff Time elapsed since last update in milliseconds
         * @return Current movement result status
         */
        virtual MovementResult Update(Player* bot, uint32 diff) = 0;

        /**
         * @brief Finalize and cleanup the movement generator
         * @param bot The bot player
         * @param interrupted True if movement was interrupted
         */
        virtual void Finalize(Player* bot, bool interrupted) = 0;

        /**
         * @brief Check if movement can be interrupted by another generator
         * @param newType The type of movement trying to interrupt
         * @param newPriority The priority of the interrupting movement
         * @return True if this movement can be interrupted
         */
        virtual bool CanBeInterrupted(MovementGeneratorType newType,
                                     MovementPriority newPriority) const;

        /**
         * @brief Handle being interrupted by another movement
         * @param bot The bot player
         * @param interruptType The type of movement that interrupted
         */
        virtual void OnInterrupted(Player* bot, MovementGeneratorType interruptType);

        /**
         * @brief Get the current movement path
         * @return Shared pointer to current path or nullptr
         */
        virtual MovementPathPtr GetPath() const { return _currentPath; }

        /**
         * @brief Notify that target position has changed
         * @param bot The bot player
         * @param newPosition New target position
         */
        virtual void OnTargetMoved(Player* bot, Position const& newPosition);

        /**
         * @brief Get movement generator type
         * @return The type of this generator
         */
        MovementGeneratorType GetType() const { return _type; }

        /**
         * @brief Get movement priority
         * @return The priority level
         */
        MovementPriority GetPriority() const { return _priority; }

        /**
         * @brief Check if movement is active
         * @return True if currently active
         */
        bool IsActive() const { return _isActive.load(); }

        /**
         * @brief Check if movement has reached destination
         * @return True if at destination
         */
        bool HasReached() const { return _hasReached.load(); }

        /**
         * @brief Get current movement state
         * @return Reference to movement state
         */
        MovementState const& GetState() const { return _state; }

        /**
         * @brief Get movement metrics for performance monitoring
         * @return Reference to metrics
         */
        MovementMetrics const& GetMetrics() const { return _metrics; }

        /**
         * @brief Set maximum update frequency
         * @param intervalMs Update interval in milliseconds
         */
        void SetUpdateInterval(uint32 intervalMs) { _updateInterval = intervalMs; }

        /**
         * @brief Enable or disable smooth movement transitions
         * @param enable True to enable smoothing
         */
        void SetSmoothTransitions(bool enable) { _smoothTransitions = enable; }

        /**
         * @brief Set maximum distance before recalculating path
         * @param distance Distance threshold
         */
        void SetRecalcThreshold(float distance) { _recalcThreshold = distance; }

    protected:
        /**
         * @brief Send movement packet to update position
         * @param bot The bot player
         * @param position Target position
         * @param speed Movement speed
         */
        void SendMovementPacket(Player* bot, Position const& position, float speed);

        /**
         * @brief Update bot facing direction
         * @param bot The bot player
         * @param angle Target angle in radians
         */
        void SetFacing(Player* bot, float angle);

        /**
         * @brief Stop all movement immediately
         * @param bot The bot player
         */
        void StopMovement(Player* bot);

        /**
         * @brief Check if enough time has passed for next update
         * @return True if should update
         */
        bool ShouldUpdate();

        /**
         * @brief Calculate distance to target
         * @param bot The bot player
         * @param target Target position or unit
         * @return Distance in yards
         */
        float GetDistanceToTarget(Player* bot, Position const& target) const;
        float GetDistanceToTarget(Player* bot, Unit* target) const;

        /**
         * @brief Check if bot is stuck
         * @param bot The bot player
         * @param currentPos Current position
         * @return True if stuck detected
         */
        bool IsStuck(Player* bot, Position const& currentPos);

        /**
         * @brief Handle stuck situation
         * @param bot The bot player
         */
        virtual void HandleStuck(Player* bot);

        /**
         * @brief Update performance metrics
         * @param cpuMicros CPU time used in microseconds
         * @param pathNodes Number of path nodes
         */
        void UpdateMetrics(uint32 cpuMicros, uint32 pathNodes);

        /**
         * @brief Integrate with TrinityCore MotionMaster
         * @param bot The bot player
         * @return Pointer to motion master
         */
        MotionMaster* GetMotionMaster(Player* bot) const;

        /**
         * @brief Validate movement destination
         * @param bot The bot player
         * @param destination Target position
         * @return True if destination is valid
         */
        bool ValidateDestination(Player* bot, Position const& destination) const;

        /**
         * @brief Apply movement flags based on terrain
         * @param bot The bot player
         * @param terrain Type of terrain
         */
        void ApplyMovementFlags(Player* bot, TerrainType terrain);

        // Member variables
        MovementGeneratorType _type;
        MovementPriority _priority;
        MovementState _state;
        MovementMetrics _metrics;
        MovementPathPtr _currentPath;

        std::atomic<bool> _isActive;
        std::atomic<bool> _hasReached;
        std::atomic<bool> _needsPath;

        uint32 _updateInterval;
        float _recalcThreshold;
        bool _smoothTransitions;

        std::chrono::steady_clock::time_point _lastUpdate;
        std::chrono::steady_clock::time_point _initTime;
        Position _lastPosition;
        uint32 _positionCheckTimer;
        uint32 _pathRecalcTimer;

    private:
        // Disable copy/move
        MovementGenerator(MovementGenerator const&) = delete;
        MovementGenerator& operator=(MovementGenerator const&) = delete;
        MovementGenerator(MovementGenerator&&) = delete;
        MovementGenerator& operator=(MovementGenerator&&) = delete;
    };
}

#endif // TRINITY_PLAYERBOT_MOVEMENT_GENERATOR_H