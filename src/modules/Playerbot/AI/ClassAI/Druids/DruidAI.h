/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "ClassAI.h"
#include "DruidSpecialization.h"
#include <memory>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <queue>
#include <mutex>

namespace Playerbot
{

class DruidSpecialization;

class TC_GAME_API DruidAI : public ClassAI
{
public:
    explicit DruidAI(Player* bot);
    ~DruidAI() = default;

    // ClassAI interface implementation
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat state callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

protected:
    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

private:
    // Specialization management
    void DetectSpecialization();
    void InitializeSpecialization();
    DruidSpec GetCurrentSpecialization() const;

    // Enhanced druid mechanics
    void OptimizeFormShifting();
    void ManageShapeshiftCosts();
    void OptimizeResourceManagement();
    void HandleMultiFormCombat();
    void ManageDruidCooldowns();
    void OptimizePositioningByForm();
    void HandleFormSpecificBuffs();
    void ManageEnergyManaBalance();
    void OptimizeFormTransitions();
    void HandleHybridCasting();
    void ManageDruidUtilities();

    // Combat system integration
    std::unique_ptr<class BotThreatManager> _threatManager;
    std::unique_ptr<class TargetSelector> _targetSelector;
    std::unique_ptr<class PositionManager> _positionManager;
    std::unique_ptr<class InterruptManager> _interruptManager;

    // Enhanced performance tracking
    struct DruidMetrics {
        std::atomic<uint32> formShifts{0};
        std::atomic<uint32> healingDone{0};
        std::atomic<uint32> damageDone{0};
        std::atomic<uint32> manaSpent{0};
        std::atomic<uint32> energySpent{0};
        std::atomic<uint32> hotApplications{0};
        std::atomic<uint32> dotApplications{0};
        std::atomic<float> formUptime[4]{0.0f, 0.0f, 0.0f, 0.0f}; // Caster, Bear, Cat, Moonkin
        std::atomic<float> hybridEfficiency{0.0f};
        std::chrono::steady_clock::time_point combatStartTime;
        std::chrono::steady_clock::time_point lastUpdate;
        std::chrono::steady_clock::time_point formShiftTime;
        DruidForm currentTrackedForm{DruidForm::CASTER};
        void Reset() {
            formShifts = 0; healingDone = 0; damageDone = 0;
            manaSpent = 0; energySpent = 0; hotApplications = 0; dotApplications = 0;
            for (int i = 0; i < 4; ++i) formUptime[i] = 0.0f;
            hybridEfficiency = 0.0f;
            combatStartTime = std::chrono::steady_clock::now();
            lastUpdate = combatStartTime;
            formShiftTime = combatStartTime;
            currentTrackedForm = DruidForm::CASTER;
        }
        void TrackFormShift(DruidForm newForm) {
            auto now = std::chrono::steady_clock::now();
            auto timeInForm = std::chrono::duration_cast<std::chrono::milliseconds>(now - formShiftTime);

            // Update uptime for previous form
            int formIndex = static_cast<int>(currentTrackedForm);
            if (formIndex < 4) {
                float currentUptime = formUptime[formIndex].load();
                formUptime[formIndex] = currentUptime + timeInForm.count();
            }

            currentTrackedForm = newForm;
            formShiftTime = now;
            formShifts++;
        }
    } _druidMetrics;

    // Form management enhancement
    std::atomic<DruidForm> _currentForm{DruidForm::CASTER};
    std::atomic<DruidForm> _previousForm{DruidForm::CASTER};
    std::atomic<uint32> _lastFormShift{0};
    std::atomic<bool> _isShifting{false};
    mutable std::mutex _formMutex;

    // Resource tracking
    std::atomic<uint32> _manaBeforeShift{0};
    std::atomic<uint32> _energyBeforeShift{0};
    std::atomic<uint32> _rageBeforeShift{0};

    // Utility management
    std::atomic<bool> _innervateReady{true};
    std::atomic<bool> _battleResReady{true};
    std::atomic<uint32> _lastInnervate{0};
    std::atomic<uint32> _lastBattleRes{0};

    // Specialization delegation
    std::unique_ptr<DruidSpecialization> _specialization;
    DruidSpec _detectedSpec;

    // Enhanced constants
    static constexpr uint32 FORM_SHIFT_COOLDOWN = 1500; // 1.5 seconds global cooldown
    static constexpr uint32 METRICS_UPDATE_INTERVAL = 500; // 0.5 seconds
    static constexpr float HYBRID_EFFICIENCY_THRESHOLD = 0.7f; // 70% efficiency
    static constexpr uint32 INNERVATE_COOLDOWN = 360000; // 6 minutes
    static constexpr uint32 BATTLE_RES_COOLDOWN = 1800000; // 30 minutes
    static constexpr float MANA_SHIFT_THRESHOLD = 0.2f; // 20% mana threshold
    static constexpr float OPTIMAL_FORM_SWITCH_TIME = 3000; // 3 seconds minimum
};

} // namespace Playerbot