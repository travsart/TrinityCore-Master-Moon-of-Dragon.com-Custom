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
#include <unordered_map>

struct MigrationInfo
{
    std::string version;
    std::string description;
    std::string filename;
    std::function<bool()> upgradeFunction;
    std::function<bool()> downgradeFunction;
    bool isApplied = false;
    uint32 executionTimeMs = 0;
    std::string checksum;
};

class TC_GAME_API PlayerbotMigrationMgr
{
public:
    static PlayerbotMigrationMgr* instance();

    // Core migration operations
    bool Initialize();
    bool ApplyMigrations();
    bool ApplyMigration(std::string const& version);
    bool RollbackMigration(std::string const& version);

    // Migration information
    std::vector<std::string> GetPendingMigrations();
    std::vector<std::string> GetAppliedMigrations();
    std::string GetCurrentVersion();
    bool IsMigrationApplied(std::string const& version);

    // Database schema validation
    bool ValidateSchema();
    bool ValidateVersion(std::string const& expectedVersion);
    bool CreateMigrationTable();

    // Migration registration
    void RegisterMigration(MigrationInfo const& migration);

    // Utility functions
    std::string CalculateFileChecksum(std::string const& filepath);
    bool ExecuteSQLFile(std::string const& filepath);
    bool ExecuteSQLStatement(std::string const& sql);

    // Safety and rollback
    bool BackupDatabase(std::string const& backupPath = "");
    bool RestoreDatabase(std::string const& backupPath);
    bool CanRollback(std::string const& version);

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

    MigrationStatus GetMigrationStatus();
    void PrintMigrationStatus();

private:
    PlayerbotMigrationMgr() = default;
    ~PlayerbotMigrationMgr() = default;

    // Internal migration management
    bool LoadMigrationsFromDatabase();
    bool LoadMigrationFiles();
    bool ValidateMigrationIntegrity();

    // Database operations
    bool RecordMigration(std::string const& version, std::string const& description, uint32 executionTimeMs = 0, std::string const& checksum = "");
    bool RemoveMigrationRecord(std::string const& version);

    // Version comparison
    int CompareVersions(std::string const& version1, std::string const& version2);
    std::vector<std::string> GetVersionSequence(std::string const& fromVersion, std::string const& toVersion);

    // Error handling
    void LogMigrationError(std::string const& version, std::string const& error);
    void LogMigrationSuccess(std::string const& version, uint32 executionTimeMs);

    // Data members
    std::unordered_map<std::string, MigrationInfo> _migrations;
    std::vector<std::string> _appliedMigrations;
    std::string _currentVersion;
    bool _initialized = false;

    // Migration file paths
    static constexpr char const* MIGRATION_PATH = "sql/migrations/";
    static constexpr char const* BACKUP_PATH = "sql/backups/";
    static constexpr char const* MIGRATION_TABLE = "playerbot_migrations";

    // Supported migration versions (in order)
    static std::vector<std::string> const MIGRATION_SEQUENCE;
};

// Migration helper macros
#define REGISTER_MIGRATION(version, description, upgradeFunc, downgradeFunc) \
    do { \
        MigrationInfo migration; \
        migration.version = version; \
        migration.description = description; \
        migration.upgradeFunction = upgradeFunc; \
        migration.downgradeFunction = downgradeFunc; \
        PlayerbotMigrationMgr::instance()->RegisterMigration(migration); \
    } while(0)

#define MIGRATION_LOG_INFO(version, message, ...) \
    TC_LOG_INFO("playerbots.migration", "[Migration {}] " message, version, ##__VA_ARGS__)

#define MIGRATION_LOG_ERROR(version, message, ...) \
    TC_LOG_ERROR("playerbots.migration", "[Migration {}] ERROR: " message, version, ##__VA_ARGS__)

#define MIGRATION_LOG_WARN(version, message, ...) \
    TC_LOG_WARN("playerbots.migration", "[Migration {}] WARNING: " message, version, ##__VA_ARGS__)