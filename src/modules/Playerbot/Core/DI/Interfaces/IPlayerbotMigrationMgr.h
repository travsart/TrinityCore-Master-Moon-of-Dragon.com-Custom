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

// Forward declaration
struct MigrationInfo;

/**
 * @brief Interface for database migration management
 *
 * Manages database schema migrations with support for versioning,
 * rollback, validation, and automatic migration discovery from SQL files.
 */
class TC_GAME_API IPlayerbotMigrationMgr
{
public:
    virtual ~IPlayerbotMigrationMgr() = default;

    // Core migration operations
    virtual bool Initialize() = 0;
    virtual bool ApplyMigrations() = 0;
    virtual bool ApplyMigration(std::string const& version) = 0;
    virtual bool RollbackMigration(std::string const& version) = 0;

    // Migration information
    virtual std::vector<std::string> GetPendingMigrations() = 0;
    virtual std::vector<std::string> GetAppliedMigrations() = 0;
    virtual std::string GetCurrentVersion() = 0;
    virtual bool IsMigrationApplied(std::string const& version) = 0;

    // Database schema validation
    virtual bool ValidateSchema() = 0;
    virtual bool ValidateVersion(std::string const& expectedVersion) = 0;
    virtual bool CreateMigrationTable() = 0;

    // Migration registration
    virtual void RegisterMigration(MigrationInfo const& migration) = 0;

    // Utility functions
    virtual std::string CalculateFileChecksum(std::string const& filepath) = 0;
    virtual bool ExecuteSQLFile(std::string const& filepath) = 0;
    virtual bool ExecuteSQLStatement(std::string const& sql) = 0;

    // Safety and rollback
    virtual bool BackupDatabase(std::string const& backupPath = "") = 0;
    virtual bool RestoreDatabase(std::string const& backupPath) = 0;
    virtual bool CanRollback(std::string const& version) = 0;

    // Migration status and reporting
    struct MigrationStatus
    {
        std::string currentVersion;
        std::string targetVersion;
        uint32 pendingCount;
        uint32 appliedCount;
        std::vector<std::string> pendingMigrations;
        std::vector<std::string> failedMigrations;
        bool isValid;
    };

    virtual MigrationStatus GetMigrationStatus() = 0;
    virtual void PrintMigrationStatus() = 0;
};
