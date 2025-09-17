/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotMigrationMgr.h"
#include "PlayerbotDatabaseStatements.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include "Util.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

// Define migration sequence
std::vector<std::string> const PlayerbotMigrationMgr::MIGRATION_SEQUENCE =
{
    "001",  // Initial schema
    "002",  // Account management
    "003"   // Lifecycle management
};

PlayerbotMigrationMgr* PlayerbotMigrationMgr::instance()
{
    static PlayerbotMigrationMgr instance;
    return &instance;
}

bool PlayerbotMigrationMgr::Initialize()
{
    if (_initialized)
        return true;

    TC_LOG_INFO("playerbots.migration", "Initializing Playerbot Migration Manager...");

    // Create migration table if it doesn't exist
    if (!CreateMigrationTable())
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to create migration tracking table");
        return false;
    }

    // Load applied migrations from database
    if (!LoadMigrationsFromDatabase())
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to load migration history from database");
        return false;
    }

    // Load migration files from filesystem
    if (!LoadMigrationFiles())
    {
        TC_LOG_WARN("playerbots.migration", "No migration files found or failed to load");
    }

    // Validate migration integrity
    if (!ValidateMigrationIntegrity())
    {
        TC_LOG_ERROR("playerbots.migration", "Migration integrity validation failed");
        return false;
    }

    // Determine current version
    _currentVersion = GetCurrentVersion();

    _initialized = true;
    TC_LOG_INFO("playerbots.migration", "Migration Manager initialized. Current version: {}", _currentVersion);

    return true;
}

bool PlayerbotMigrationMgr::CreateMigrationTable()
{
    std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS `)" + std::string(MIGRATION_TABLE) + R"(` (
            `version` VARCHAR(20) NOT NULL,
            `description` VARCHAR(255) NOT NULL,
            `applied_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            `checksum` VARCHAR(64) NULL,
            `execution_time_ms` INT UNSIGNED NULL,
            PRIMARY KEY (`version`),
            INDEX `idx_applied` (`applied_at`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        COMMENT='Database migration tracking'
    )";

    return ExecuteSQLStatement(createTableSQL);
}

bool PlayerbotMigrationMgr::LoadMigrationsFromDatabase()
{
    std::string query = "SELECT version, description, checksum, execution_time_ms FROM " + std::string(MIGRATION_TABLE) + " ORDER BY applied_at";

    QueryResult result = CharacterDatabase.Query(query.c_str());
    if (!result)
        return true; // No migrations applied yet, which is valid

    _appliedMigrations.clear();

    do
    {
        Field* fields = result->Fetch();
        std::string version = fields[0].GetString();
        std::string description = fields[1].GetString();
        std::string checksum = fields[2].GetString();
        uint32 executionTime = fields[3].GetUInt32();

        _appliedMigrations.push_back(version);

        // Update migration info if we have it registered
        if (_migrations.find(version) != _migrations.end())
        {
            _migrations[version].isApplied = true;
            _migrations[version].executionTimeMs = executionTime;
            _migrations[version].checksum = checksum;
        }

        MIGRATION_LOG_INFO(version, "Loaded applied migration: {}", description);
    }
    while (result->NextRow());

    std::sort(_appliedMigrations.begin(), _appliedMigrations.end(), [this](std::string const& a, std::string const& b) {
        return CompareVersions(a, b) < 0;
    });

    return true;
}

bool PlayerbotMigrationMgr::LoadMigrationFiles()
{
    std::filesystem::path migrationDir = std::filesystem::path(MIGRATION_PATH);

    if (!std::filesystem::exists(migrationDir))
    {
        MIGRATION_LOG_WARN("SYSTEM", "Migration directory does not exist: {}", migrationDir.string());
        return false;
    }

    for (auto const& entry : std::filesystem::directory_iterator(migrationDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sql")
        {
            std::string filename = entry.path().filename().string();

            // Extract version from filename (format: XXX_to_YYY_description.sql)
            size_t firstUnderscore = filename.find('_');
            if (firstUnderscore == std::string::npos)
                continue;

            std::string fromVersion = filename.substr(0, firstUnderscore);

            size_t toPos = filename.find("_to_");
            if (toPos == std::string::npos)
                continue;

            size_t secondUnderscore = filename.find('_', toPos + 4);
            if (secondUnderscore == std::string::npos)
                continue;

            std::string toVersion = filename.substr(toPos + 4, secondUnderscore - toPos - 4);

            // Register migration if not already registered
            if (_migrations.find(toVersion) == _migrations.end())
            {
                MigrationInfo migration;
                migration.version = toVersion;
                migration.filename = entry.path().string();
                migration.checksum = CalculateFileChecksum(migration.filename);
                migration.description = "File-based migration to version " + toVersion;

                _migrations[toVersion] = migration;
                MIGRATION_LOG_INFO(toVersion, "Registered migration file: {}", filename);
            }
        }
    }

    return true;
}

bool PlayerbotMigrationMgr::ValidateMigrationIntegrity()
{
    // Check if applied migrations exist in our migration sequence
    for (std::string const& appliedVersion : _appliedMigrations)
    {
        auto it = std::find(MIGRATION_SEQUENCE.begin(), MIGRATION_SEQUENCE.end(), appliedVersion);
        if (it == MIGRATION_SEQUENCE.end())
        {
            MIGRATION_LOG_WARN(appliedVersion, "Applied migration not found in expected sequence");
        }
    }

    // Check for gaps in migration sequence
    for (size_t i = 0; i < _appliedMigrations.size(); ++i)
    {
        size_t expectedIndex = i;
        if (expectedIndex >= MIGRATION_SEQUENCE.size())
            break;

        std::string const& expectedVersion = MIGRATION_SEQUENCE[expectedIndex];
        if (_appliedMigrations[i] != expectedVersion)
        {
            MIGRATION_LOG_WARN("SYSTEM", "Migration sequence gap detected. Expected: {}, Found: {}",
                expectedVersion, _appliedMigrations[i]);
        }
    }

    return true;
}

std::string PlayerbotMigrationMgr::GetCurrentVersion()
{
    if (_appliedMigrations.empty())
        return "000"; // No migrations applied

    // Return the highest applied version
    return *std::max_element(_appliedMigrations.begin(), _appliedMigrations.end(),
        [this](std::string const& a, std::string const& b) {
            return CompareVersions(a, b) < 0;
        });
}

bool PlayerbotMigrationMgr::ApplyMigrations()
{
    if (!_initialized && !Initialize())
        return false;

    std::vector<std::string> pendingMigrations = GetPendingMigrations();

    if (pendingMigrations.empty())
    {
        TC_LOG_INFO("playerbots.migration", "No pending migrations to apply");
        return true;
    }

    TC_LOG_INFO("playerbots.migration", "Applying {} pending migrations", pendingMigrations.size());

    bool success = true;
    for (std::string const& version : pendingMigrations)
    {
        if (!ApplyMigration(version))
        {
            MIGRATION_LOG_ERROR(version, "Failed to apply migration");
            success = false;
            break; // Stop on first failure
        }
    }

    return success;
}

bool PlayerbotMigrationMgr::ApplyMigration(std::string const& version)
{
    if (!_initialized)
        return false;

    if (IsMigrationApplied(version))
    {
        MIGRATION_LOG_WARN(version, "Migration already applied");
        return true;
    }

    auto it = _migrations.find(version);
    if (it == _migrations.end())
    {
        MIGRATION_LOG_ERROR(version, "Migration not registered");
        return false;
    }

    MigrationInfo& migration = it->second;

    MIGRATION_LOG_INFO(version, "Applying migration: {}", migration.description);

    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = false;

    // Try to execute registered function first
    if (migration.upgradeFunction)
    {
        try
        {
            success = migration.upgradeFunction();
        }
        catch (std::exception const& ex)
        {
            MIGRATION_LOG_ERROR(version, "Exception during migration function: {}", ex.what());
            success = false;
        }
    }
    // Otherwise try to execute SQL file
    else if (!migration.filename.empty())
    {
        success = ExecuteSQLFile(migration.filename);
    }
    else
    {
        MIGRATION_LOG_ERROR(version, "No upgrade method available");
        success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    uint32 executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (success)
    {
        // Record successful migration
        if (!RecordMigration(version, migration.description, executionTime, migration.checksum))
        {
            MIGRATION_LOG_ERROR(version, "Failed to record migration in database");
            return false;
        }

        migration.isApplied = true;
        migration.executionTimeMs = executionTime;
        _appliedMigrations.push_back(version);

        // Update current version
        if (CompareVersions(version, _currentVersion) > 0)
            _currentVersion = version;

        LogMigrationSuccess(version, executionTime);
    }
    else
    {
        LogMigrationError(version, "Migration execution failed");
    }

    return success;
}

bool PlayerbotMigrationMgr::ExecuteSQLFile(std::string const& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to open migration file: {}", filepath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Split content by semicolons and execute each statement
    std::vector<std::string> statements;
    std::stringstream ss(content);
    std::string statement;

    while (std::getline(ss, statement, ';'))
    {
        // Trim whitespace and skip empty statements
        // Trim whitespace manually
        size_t start = statement.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            statement = "";
        } else {
            size_t end = statement.find_last_not_of(" \t\r\n");
            statement = statement.substr(start, end - start + 1);
        }
        if (statement.empty() || statement.find_first_not_of(" \t\r\n") == std::string::npos)
            continue;

        // Skip comments
        if (statement.substr(0, 2) == "--" || statement.substr(0, 2) == "/*")
            continue;

        if (!ExecuteSQLStatement(statement + ";"))
        {
            TC_LOG_ERROR("playerbots.migration", "Failed to execute statement in file {}: {}", filepath, statement.substr(0, 100));
            return false;
        }
    }

    return true;
}

bool PlayerbotMigrationMgr::ExecuteSQLStatement(std::string const& sql)
{
    try
    {
        CharacterDatabase.Execute(sql.c_str());
        return true;
    }
    catch (std::exception const& ex)
    {
        TC_LOG_ERROR("playerbots.migration", "SQL execution failed: {} | Error: {}", sql.substr(0, 200), ex.what());
        return false;
    }
}

bool PlayerbotMigrationMgr::RecordMigration(std::string const& version, std::string const& description, uint32 executionTimeMs, std::string const& checksum)
{
    std::string insertSQL = "INSERT INTO " + std::string(MIGRATION_TABLE) +
        " (version, description, execution_time_ms, checksum) VALUES ('" + version + "', '" + description + "', " +
        std::to_string(executionTimeMs) + ", '" + checksum + "') ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP";

    return ExecuteSQLStatement(insertSQL);
}

std::vector<std::string> PlayerbotMigrationMgr::GetPendingMigrations()
{
    std::vector<std::string> pending;

    for (std::string const& version : MIGRATION_SEQUENCE)
    {
        if (!IsMigrationApplied(version))
            pending.push_back(version);
    }

    return pending;
}

std::vector<std::string> PlayerbotMigrationMgr::GetAppliedMigrations()
{
    return _appliedMigrations;
}

bool PlayerbotMigrationMgr::IsMigrationApplied(std::string const& version)
{
    return std::find(_appliedMigrations.begin(), _appliedMigrations.end(), version) != _appliedMigrations.end();
}

int PlayerbotMigrationMgr::CompareVersions(std::string const& version1, std::string const& version2)
{
    // Simple numeric comparison for versions like "001", "002", "003"
    int v1 = std::stoi(version1);
    int v2 = std::stoi(version2);

    if (v1 < v2) return -1;
    if (v1 > v2) return 1;
    return 0;
}

std::string PlayerbotMigrationMgr::CalculateFileChecksum(std::string const& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return "";

    // Simple hash calculation (in real implementation, use proper hash like SHA256)
    std::hash<std::string> hasher;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return std::to_string(hasher(content));
}

void PlayerbotMigrationMgr::RegisterMigration(MigrationInfo const& migration)
{
    _migrations[migration.version] = migration;
    MIGRATION_LOG_INFO(migration.version, "Registered migration: {}", migration.description);
}

void PlayerbotMigrationMgr::LogMigrationSuccess(std::string const& version, uint32 executionTimeMs)
{
    MIGRATION_LOG_INFO(version, "Migration applied successfully in {}ms", executionTimeMs);
}

void PlayerbotMigrationMgr::LogMigrationError(std::string const& version, std::string const& error)
{
    MIGRATION_LOG_ERROR(version, "Migration failed: {}", error);
}

PlayerbotMigrationMgr::MigrationStatus PlayerbotMigrationMgr::GetMigrationStatus()
{
    MigrationStatus status;
    status.currentVersion = GetCurrentVersion();
    status.targetVersion = MIGRATION_SEQUENCE.empty() ? "000" : MIGRATION_SEQUENCE.back();

    std::vector<std::string> pending = GetPendingMigrations();
    status.pendingCount = static_cast<uint32>(pending.size());
    status.appliedCount = static_cast<uint32>(_appliedMigrations.size());
    status.pendingMigrations = pending;
    status.isValid = ValidateMigrationIntegrity();

    return status;
}

void PlayerbotMigrationMgr::PrintMigrationStatus()
{
    MigrationStatus status = GetMigrationStatus();

    TC_LOG_INFO("playerbots.migration", "=== Playerbot Migration Status ===");
    TC_LOG_INFO("playerbots.migration", "Current Version: {}", status.currentVersion);
    TC_LOG_INFO("playerbots.migration", "Target Version:  {}", status.targetVersion);
    TC_LOG_INFO("playerbots.migration", "Applied:         {} migrations", status.appliedCount);
    TC_LOG_INFO("playerbots.migration", "Pending:         {} migrations", status.pendingCount);
    TC_LOG_INFO("playerbots.migration", "Status:          {}", status.isValid ? "Valid" : "Invalid");

    if (!status.pendingMigrations.empty())
    {
        std::string pendingList;
        for (size_t i = 0; i < status.pendingMigrations.size(); ++i) {
            if (i > 0) pendingList += ", ";
            pendingList += status.pendingMigrations[i];
        }
        TC_LOG_INFO("playerbots.migration", "Pending migrations: {}", pendingList);
    }
}