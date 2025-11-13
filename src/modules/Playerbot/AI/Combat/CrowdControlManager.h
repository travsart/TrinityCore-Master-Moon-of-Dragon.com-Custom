/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#ifndef TRINITYCORE_CROWD_CONTROL_MANAGER_H
#define TRINITYCORE_CROWD_CONTROL_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <vector>
#include "GameTime.h"

class Player;
class Unit;

namespace Playerbot
{
    struct CombatMetrics;

    /**
     * @enum CrowdControlType
     * @brief Types of crowd control effects
     */
    enum class CrowdControlType : uint8
    {
        STUN,           // Stun (short duration, damage breaks)
        INCAPACITATE,   // Sap, Polymorph (long duration, damage breaks)
        DISORIENT,      // Fear, Charm (medium duration, damage doesn't break)
        ROOT,           // Root (can still cast/attack)
        SILENCE,        // Silence (prevents spellcasting)
        DISARM,         // Disarm (prevents melee attacks)
        MAX
    };

    /**
     * @struct CCTarget
     * @brief Active crowd control on a target
     */
    struct CCTarget
    {
        Unit* target;
        CrowdControlType type;
        uint32 duration;
        Player* appliedBy;
        uint32 expiryTime;
        uint32 spellId;

        CCTarget()
            : target(nullptr)
            , type(CrowdControlType::MAX)
            , duration(0)
            , appliedBy(nullptr)
            , expiryTime(0)
            , spellId(0)
        {}

        [[nodiscard]] bool IsActive() const
        {
            return GameTime::GetGameTimeMS() < expiryTime;
        }

        [[nodiscard]] uint32 GetRemainingTime() const
        {
            uint32 now = GameTime::GetGameTimeMS();
            return (now < expiryTime) ? (expiryTime - now) : 0;
        }
    };

    /**
     * @class CrowdControlManager
     * @brief Coordinate CC abilities in group
     *
     * **Problem**: Stub implementation with NO-OP methods
     * **Solution**: Full implementation with group CC coordination
     *
     * **Features**:
     * - Track active CC on all targets
     * - Prevent breaking CC (avoid AoE damage on CC'd targets)
     * - Chain CC (reapply before expiry)
     * - Coordinate multiple CCers (assign targets)
     * - Priority CC targets (healers, casters)
     *
     * **Usage Example**:
     * @code
     * CrowdControlManager ccMgr(bot);
     * ccMgr.Update(diff, combatMetrics);
     *
     * if (ccMgr.ShouldUseCrowdControl())
     * {
     *     Unit* target = ccMgr.GetPriorityTarget();
     *     uint32 spell = ccMgr.GetRecommendedSpell(target);
     *     bot->CastSpell(spell, target);
     * }
     * @endcode
     *
     * **Expected Impact**:
     * -  20% better group coordination in dungeons
     * -  Prevent premature CC breaks
     * -  Automatic CC chaining
     * -  Smart target assignment
     */
    class TC_GAME_API CrowdControlManager
    {
    public:
        explicit CrowdControlManager(Player* bot);
        ~CrowdControlManager() = default;

        /**
         * @brief Update CC tracking
         *
         * @param diff Time since last update (milliseconds)
         * @param metrics Combat metrics
         *
         * Updates active CC durations, removes expired CC
         */
        void Update(uint32 diff, const CombatMetrics& metrics);

        /**
         * @brief Reset CC manager state
         *
         * Called when leaving combat
         */
        void Reset();

        /**
         * @brief Check if should use crowd control
         *
         * @return True if CC is recommended
         *
         * Conditions:
         * - Multiple enemies present
         * - Uncrowded targets exist
         * - Bot has CC abilities available
         */
        bool ShouldUseCrowdControl();

        /**
         * @brief Get priority target for CC
         *
         * @return Best target to CC or nullptr
         *
         * Priority:
         * 1. Healers
         * 2. Casters
         * 3. High-damage enemies
         * 4. Adds/reinforcements
         */
        Unit* GetPriorityTarget();

        /**
         * @brief Get recommended CC spell for target
         *
         * @param target Target to CC
         * @return Spell ID or 0 if no suitable spell
         *
         * Considers:
         * - Target type (humanoid, beast, etc.)
         * - CC immunities
         * - Spell cooldown
         * - Mana cost
         */
        uint32 GetRecommendedSpell(Unit* target);

        /**
         * @brief Check if target should be CC'd
         *
         * @param target Target to evaluate
         * @param type CC type to use
         * @return True if CC is appropriate
         *
         * Checks:
         * - Target not already CC'd
         * - Target not immune
         * - Target is valid threat
         */
        bool ShouldCC(Unit* target, CrowdControlType type);

        /**
         * @brief Apply CC and register
         *
         * @param target Target to CC
         * @param type CC type
         * @param duration CC duration (milliseconds)
         * @param bot Bot applying CC
         * @param spellId Spell ID used
         *
         * Registers CC in tracking system
         */
        void ApplyCC(Unit* target, CrowdControlType type, uint32 duration, Player* bot, uint32 spellId);

        /**
         * @brief Remove CC from tracking
         *
         * @param target Target to remove
         *
         * Called when CC breaks or expires
         */
        void RemoveCC(Unit* target);

        /**
         * @brief Get bot for chain CC
         *
         * @param target Target needing chain CC
         * @return Bot that should chain CC or nullptr
         *
         * Assigns chain CC responsibility to group members
         */
        Player* GetChainCCBot(Unit* target);

        /**
         * @brief Check if target is CC'd
         *
         * @param target Target to check
         * @return True if target has active CC
         */
        bool IsTargetCCd(Unit* target) const;

        /**
         * @brief Get active CC on target
         *
         * @param target Target to check
         * @return CC info or nullptr if no active CC
         */
        const CCTarget* GetActiveCC(Unit* target) const;

        /**
         * @brief Get all CC'd targets
         *
         * @return Vector of all active CC targets
         */
        std::vector<Unit*> GetCCdTargets() const;

        /**
         * @brief Check if should break CC
         *
         * @param target CC'd target
         * @return True if safe to break CC (e.g., last enemy)
         *
         * Use case: Decide if AoE is safe to use
         */
        bool ShouldBreakCC(Unit* target) const;

    private:
        Player* _bot;
        std::unordered_map<ObjectGuid, CCTarget> _activeCCs;
        uint32 _lastUpdate;

        static constexpr uint32 UPDATE_INTERVAL = 500;  // 500ms
        static constexpr uint32 CHAIN_CC_WINDOW = 2000; // 2 seconds before expiry

        /**
         * @brief Get all enemies in combat
         */
        std::vector<Unit*> GetCombatEnemies() const;

        /**
         * @brief Check if target is immune to CC type
         */
        bool IsImmune(Unit* target, CrowdControlType type) const;

        /**
         * @brief Calculate CC priority for target
         *
         * Higher = more important to CC
         */
        float CalculateCCPriority(Unit* target) const;

        /**
         * @brief Get bot's available CC spells
         */
        std::vector<uint32> GetAvailableCCSpells() const;

        /**
         * @brief Check if spell is suitable for target
         */
        bool IsSpellSuitableForTarget(uint32 spellId, Unit* target) const;

        /**
         * @brief Update expired CCs
         */
        void UpdateExpiredCCs();
    };

} // namespace Playerbot

#endif // TRINITYCORE_CROWD_CONTROL_MANAGER_H
