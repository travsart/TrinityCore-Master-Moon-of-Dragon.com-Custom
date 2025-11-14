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
#include <functional>
#include <cstdint>

// Forward declarations
class Player;
class Group;
class Guild;

namespace Playerbot
{

// Forward declarations
struct ValidationResult;
struct PerformanceValidation;
struct SystemHealthReport;
struct ValidationMetrics;
enum class SystemComponent : uint8;
enum class ValidationLevel : uint8;

/**
 * @brief Interface for system validation framework
 *
 * Provides thorough validation of all playerbot systems, detecting
 * inconsistencies, performance issues, and ensuring overall system health.
 */
class TC_GAME_API ISystemValidation
{
public:
    virtual ~ISystemValidation() = default;

    // Core validation framework
    virtual ValidationResult ValidateSystem(SystemComponent component, ValidationLevel level = ValidationLevel::STANDARD) = 0;
    virtual ::std::vector<ValidationResult> ValidateAllSystems(ValidationLevel level = ValidationLevel::STANDARD) = 0;
    virtual bool RunSystemHealthCheck() = 0;
    virtual void PerformSystemDiagnostics() = 0;

    // Component-specific validation
    virtual ValidationResult ValidateGroupManager(ValidationLevel level) = 0;
    virtual ValidationResult ValidateRoleAssignment(ValidationLevel level) = 0;
    virtual ValidationResult ValidateQuestAutomation(ValidationLevel level) = 0;
    virtual ValidationResult ValidateDungeonBehavior(ValidationLevel level) = 0;
    virtual ValidationResult ValidateLootDistribution(ValidationLevel level) = 0;
    virtual ValidationResult ValidateTradeSystem(ValidationLevel level) = 0;
    virtual ValidationResult ValidateAuctionHouse(ValidationLevel level) = 0;
    virtual ValidationResult ValidateGuildIntegration(ValidationLevel level) = 0;

    // Data integrity validation
    virtual bool ValidatePlayerData(::Player* player) = 0;
    virtual bool ValidateGroupData(::Group* group) = 0;
    virtual bool ValidateGuildData(::Guild* guild) = 0;
    virtual bool ValidateQuestData(::Player* player) = 0;
    virtual bool ValidateLootData(::Group* group) = 0;

    // Performance validation
    virtual PerformanceValidation ValidateSystemPerformance(SystemComponent component) = 0;
    virtual ::std::vector<PerformanceValidation> ValidateAllPerformance() = 0;
    virtual bool ValidateMemoryUsage() = 0;
    virtual bool ValidateResponseTimes() = 0;

    // Consistency validation
    virtual bool ValidateCrossSystemConsistency() = 0;
    virtual bool ValidateDataSynchronization() = 0;
    virtual bool ValidateStateConsistency(::Player* player) = 0;
    virtual bool ValidateGroupStateConsistency(::Group* group) = 0;
    virtual bool ValidateGuildStateConsistency(::Guild* guild) = 0;

    // Configuration validation
    virtual bool ValidateSystemConfiguration() = 0;
    virtual bool ValidatePlayerBotConfigurations() = 0;
    virtual bool ValidateDatabaseIntegrity() = 0;
    virtual bool ValidateModuleIntegration() = 0;

    // Runtime validation
    virtual void EnableContinuousValidation(bool enable) = 0;
    virtual void SetValidationInterval(uint32 intervalMs) = 0;
    virtual void RegisterValidationTrigger(const ::std::string& triggerName, ::std::function<bool()> validator) = 0;
    virtual void ValidateOnEvent(const ::std::string& eventName) = 0;

    // Validation reporting
    virtual SystemHealthReport GenerateHealthReport() = 0;
    virtual void ExportValidationReport(const ::std::string& filename) = 0;
    virtual void LogValidationResults(const ValidationResult& result) = 0;
    virtual ::std::vector<::std::string> GetCriticalIssues() = 0;

    // Automated fixing and recovery
    virtual bool AttemptAutomaticFix(const ValidationResult& result) = 0;
    virtual void SuggestManualFixes(const ValidationResult& result) = 0;
    virtual bool RecoverFromValidationFailure(SystemComponent component) = 0;
    virtual void RestoreSystemDefaults(SystemComponent component) = 0;

    // Validation metrics and analytics
    virtual ValidationMetrics GetValidationMetrics() = 0;

    // Advanced validation features
    virtual void SetupValidationSchedule(const ::std::string& schedule) = 0;
    virtual void ValidateAfterSystemChanges() = 0;
    virtual void ValidateBeforeCriticalOperations() = 0;
    virtual void MonitorSystemDegradation() = 0;

    // Custom validation rules
    virtual void AddCustomValidationRule(const ::std::string& ruleName, ::std::function<bool(SystemComponent)> rule) = 0;
    virtual void RemoveCustomValidationRule(const ::std::string& ruleName) = 0;
    virtual ::std::vector<::std::string> GetActiveValidationRules() = 0;

    // Update and maintenance
    virtual void Update(uint32 diff) = 0;
    virtual void ProcessValidationQueue() = 0;
    virtual void CleanupValidationData() = 0;
};

} // namespace Playerbot
