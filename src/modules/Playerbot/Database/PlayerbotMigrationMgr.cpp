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
#include "PlayerbotDatabase.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include "Util.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <chrono>

// Define migration sequence
std::vector<std::string> const PlayerbotMigrationMgr::MIGRATION_SEQUENCE =
{
    "001",  // Initial schema
    "002",  // Account management
    "003",  // Lifecycle management
    "004"   // Character distribution
};

PlayerbotMigrationMgr::PlayerbotMigrationMgr()
{
    _initialized = false;
    _currentVersion = "000";
}

std::string PlayerbotMigrationMgr::GetMigrationPath()
{
    // Try multiple paths in order of preference:
    // 1. Current working directory (development)
    // 2. Installed configuration directory (production)
    // 3. Module source directory (fallback)

    std::vector<std::string> searchPaths = {
        "sql/migrations/",                               // Development path
        "../etc/sql/migrations/",                        // Installed path relative to bin
        "../../etc/sql/migrations/",                     // Alternative installed path
        "src/modules/Playerbot/sql/migrations/",         // Source directory
        "../src/modules/Playerbot/sql/migrations/"       // Alternative source directory
    };

    for (auto const& path : searchPaths)
    {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
        {
            TC_LOG_DEBUG("playerbots.migration", "Using migration path: {}", path);
            return path;
        }
    }

    // Fallback to default path
    TC_LOG_WARN("playerbots.migration", "No migration directory found, using default: {}", MIGRATION_PATH);
    return MIGRATION_PATH;
}

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

    // Register built-in migrations
    RegisterBuiltInMigrations();

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
    TC_LOG_INFO("playerbots.migration", "Migration Manager initialized successfully. Current version: {}", _currentVersion);

    return true;
}

bool PlayerbotMigrationMgr::CreateMigrationTable()
{
    // Create migration table only if it doesn't exist (don't drop existing data!)
    std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS `)" + std::string(MIGRATION_TABLE) + R"(` (
            `version` VARCHAR(20) NOT NULL,
            `description` VARCHAR(255) NOT NULL,
            `applied_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            `checksum` VARCHAR(64) NULL,
            `execution_time_ms` INT UNSIGNED NULL DEFAULT 0,
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

    QueryResult result = sPlayerbotDatabase->Query(query);
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

        // Skip empty or invalid migration records
        if (version.empty()) {
            TC_LOG_WARN("playerbots.migration", "Skipping empty migration version");
            continue;
        }

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
    std::filesystem::path migrationDir = std::filesystem::path(GetMigrationPath());

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

    // Discover all migration files (TrinityCore pattern)
    _discoveredMigrations = DiscoverMigrationFiles();
    if (_discoveredMigrations.empty())
    {
        TC_LOG_INFO("playerbots.migration", "No migration files found in {}", GetMigrationPath());
        return true;
    }

    // Count pending migrations
    size_t pendingCount = 0;
    for (auto const& migration : _discoveredMigrations)
    {
        if (!migration.isApplied)
            ++pendingCount;
    }

    if (pendingCount == 0)
    {
        TC_LOG_INFO("playerbots.migration", "No pending migrations to apply");
        return true;
    }

    TC_LOG_INFO("playerbots.migration", "Applying {} pending migrations (discovered {} total files)", pendingCount, _discoveredMigrations.size());

    // Apply pending migrations in order
    for (auto const& migration : _discoveredMigrations)
    {
        if (!migration.isApplied)
        {
            if (!ApplyMigrationFile(migration))
            {
                TC_LOG_ERROR("playerbots.migration", "Failed to apply migration file {}", migration.filename);
                return false;
            }
        }
    }

    TC_LOG_INFO("playerbots.migration", "All pending migrations applied successfully");
    return true;
}

bool PlayerbotMigrationMgr::ApplyMigration(std::string const& version)
{
    printf("[DEBUG] ApplyMigration() called for version: %s\n", version.c_str());
    fflush(stdout);

    if (!_initialized)
    {
        printf("[DEBUG] Migration manager not initialized!\n");
        fflush(stdout);
        return false;
    }

    if (IsMigrationApplied(version))
    {
        MIGRATION_LOG_WARN(version, "Migration already applied");
        printf("[DEBUG] Migration %s already applied\n", version.c_str());
        fflush(stdout);
        return true;
    }

    printf("[DEBUG] Looking up migration %s in registry\n", version.c_str());
    fflush(stdout);
    auto it = _migrations.find(version);
    if (it == _migrations.end())
    {
        MIGRATION_LOG_ERROR(version, "Migration not registered");
        printf("[DEBUG] Migration %s not registered!\n", version.c_str());
        fflush(stdout);
        return false;
    }

    MigrationInfo& migration = it->second;

    MIGRATION_LOG_INFO(version, "Applying migration: {}", migration.description);
    printf("[DEBUG] Starting to apply migration %s: %s\n", version.c_str(), migration.description.c_str());
    fflush(stdout);

    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = false;

    // Try to execute registered function first
    if (migration.upgradeFunction)
    {
        printf("[DEBUG] Migration %s has upgrade function, calling it now...\n", version.c_str());
        fflush(stdout);
        try
        {
            success = migration.upgradeFunction();
            printf("[DEBUG] Migration %s upgrade function completed with result: %s\n", version.c_str(), success ? "SUCCESS" : "FAILURE");
            fflush(stdout);
        }
        catch (std::exception const& ex)
        {
            MIGRATION_LOG_ERROR(version, "Exception during migration function: {}", ex.what());
            printf("[DEBUG] EXCEPTION in migration %s: %s\n", version.c_str(), ex.what());
            fflush(stdout);
            success = false;
        }
    }
    // Otherwise try to execute SQL file
    else if (!migration.filename.empty())
    {
        printf("[DEBUG] Migration %s has SQL file, executing it\n", version.c_str());
        fflush(stdout);
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
        printf("[DEBUG] Migration %s succeeded, recording in database...\n", version.c_str());
        fflush(stdout);
        // Record successful migration
        if (!RecordMigration(version, migration.description, executionTime, migration.checksum))
        {
            MIGRATION_LOG_ERROR(version, "Failed to record migration in database");
            printf("[DEBUG] FAILED to record migration %s in database\n", version.c_str());
            fflush(stdout);
            return false;
        }
        printf("[DEBUG] Migration %s recorded successfully\n", version.c_str());
        fflush(stdout);

        migration.isApplied = true;
        migration.executionTimeMs = executionTime;
        _appliedMigrations.push_back(version);

        // Update current version
        if (CompareVersions(version, _currentVersion) > 0)
            _currentVersion = version;

        LogMigrationSuccess(version, executionTime);
        printf("[DEBUG] Migration %s completed successfully, returning true\n", version.c_str());
        fflush(stdout);
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
        sPlayerbotDatabase->Execute(sql);
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
    printf("[DEBUG] GetPendingMigrations() called\n");
    fflush(stdout);
    std::vector<std::string> pending;

    printf("[DEBUG] MIGRATION_SEQUENCE size: %zu\n", MIGRATION_SEQUENCE.size());
    fflush(stdout);

    for (std::string const& version : MIGRATION_SEQUENCE)
    {
        printf("[DEBUG] Checking migration version: %s\n", version.c_str());
        fflush(stdout);
        if (!IsMigrationApplied(version))
        {
            printf("[DEBUG] Migration %s is pending\n", version.c_str());
            fflush(stdout);
            pending.push_back(version);
        }
        else
        {
            printf("[DEBUG] Migration %s is already applied\n", version.c_str());
            fflush(stdout);
        }
    }

    printf("[DEBUG] GetPendingMigrations() returning %zu pending migrations\n", pending.size());
    fflush(stdout);
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
    // Handle empty versions (treat as "000")
    if (version1.empty() && version2.empty()) return 0;
    if (version1.empty()) return -1;  // Empty comes before any version
    if (version2.empty()) return 1;   // Any version comes after empty

    try {
        // Simple numeric comparison for versions like "001", "002", "003"
        int v1 = std::stoi(version1);
        int v2 = std::stoi(version2);

        if (v1 < v2) return -1;
        if (v1 > v2) return 1;
        return 0;
    }
    catch (std::exception const& e) {
        printf("[DEBUG] ERROR in CompareVersions: v1='%s', v2='%s', exception: %s\n", version1.c_str(), version2.c_str(), e.what());
        fflush(stdout);
        // Fallback to string comparison if numeric conversion fails
        return version1.compare(version2);
    }
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

void PlayerbotMigrationMgr::RegisterBuiltInMigrations()
{
    TC_LOG_INFO("playerbots.migration", "Registering built-in migrations...");

    // Migration 001: Initial schema - create all basic tables
    {
        MigrationInfo migration;
        migration.version = "001";
        migration.description = "Initial Playerbot schema - create core tables";
        migration.upgradeFunction = [this]() -> bool {
            return ApplyInitialSchema();
        };
        migration.downgradeFunction = [this]() -> bool {
            return DropAllTables();
        };
        RegisterMigration(migration);
    }

    // Migration 002: Account management enhancements
    {
        MigrationInfo migration;
        migration.version = "002";
        migration.description = "Account management system enhancements";
        migration.upgradeFunction = [this]() -> bool {
            return ApplyAccountEnhancements();
        };
        migration.downgradeFunction = [this]() -> bool {
            return DropAccountEnhancements();
        };
        RegisterMigration(migration);
    }

    // Migration 003: Lifecycle management
    {
        MigrationInfo migration;
        migration.version = "003";
        migration.description = "Bot lifecycle management system";
        migration.upgradeFunction = [this]() -> bool {
            return ApplyLifecycleManagement();
        };
        migration.downgradeFunction = [this]() -> bool {
            return DropLifecycleManagement();
        };
        RegisterMigration(migration);
    }

    // Migration 004: Character distribution
    {
        MigrationInfo migration;
        migration.version = "004";
        migration.description = "Character distribution system";
        migration.upgradeFunction = [this]() -> bool {
            return ApplyCharacterDistribution();
        };
        migration.downgradeFunction = [this]() -> bool {
            return DropCharacterDistribution();
        };
        RegisterMigration(migration);
    }

    TC_LOG_INFO("playerbots.migration", "Registered {} built-in migrations", _migrations.size());
}

bool PlayerbotMigrationMgr::ApplyInitialSchema()
{
    TC_LOG_INFO("playerbots.migration", "Applying initial schema from SQL file...");

    std::string sqlFile = GetMigrationPath() + "001_initial_schema.sql";
    if (!ExecuteSQLFile(sqlFile))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to execute initial schema SQL file: {}", sqlFile);
        return false;
    }

    TC_LOG_INFO("playerbots.migration", "Initial schema applied successfully from SQL file");
    return true;
}

bool PlayerbotMigrationMgr::DropAllTables()
{
    TC_LOG_INFO("playerbots.migration", "Dropping all playerbot tables...");

    std::vector<std::string> dropStatements = {
        "DROP TABLE IF EXISTS `playerbots_names_used`",
        "DROP TABLE IF EXISTS `playerbots_names`",
        "DROP TABLE IF EXISTS `playerbot_activity_patterns`"
    };

    for (const auto& sql : dropStatements)
    {
        ExecuteSQLStatement(sql); // Don't fail on drop errors
    }

    return true;
}

bool PlayerbotMigrationMgr::ApplyAccountEnhancements()
{
    TC_LOG_INFO("playerbots.migration", "Applying account management enhancements from SQL file...");

    std::string sqlFile = GetMigrationPath() + "002_account_management.sql";
    if (!ExecuteSQLFile(sqlFile))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to execute account management SQL file: {}", sqlFile);
        return false;
    }

    TC_LOG_INFO("playerbots.migration", "Account management enhancement migration completed successfully");
    return true;
}

bool PlayerbotMigrationMgr::DropAccountEnhancements()
{
    TC_LOG_INFO("playerbots.migration", "Dropping account management enhancements...");
    // Placeholder for future rollback
    return true;
}

bool PlayerbotMigrationMgr::ApplyLifecycleManagement()
{
    TC_LOG_INFO("playerbots.migration", "Applying lifecycle management system from SQL file...");

    std::string sqlFile = GetMigrationPath() + "003_lifecycle_management.sql";
    if (!ExecuteSQLFile(sqlFile))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to execute lifecycle management SQL file: {}", sqlFile);
        return false;
    }

    TC_LOG_INFO("playerbots.migration", "Lifecycle management system migration completed successfully");
    return true;
}

bool PlayerbotMigrationMgr::DropLifecycleManagement()
{
    TC_LOG_INFO("playerbots.migration", "Dropping lifecycle management system...");
    // Placeholder for future rollback
    return true;
}

bool PlayerbotMigrationMgr::ApplyCharacterDistribution()
{
    TC_LOG_INFO("playerbots.migration", "Applying character distribution system from SQL file...");

    std::string sqlFile = GetMigrationPath() + "004_character_distribution.sql";
    if (!ExecuteSQLFile(sqlFile))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to execute character distribution SQL file: {}", sqlFile);
        return false;
    }

    TC_LOG_INFO("playerbots.migration", "Character distribution system migration completed successfully");
    return true;
}

bool PlayerbotMigrationMgr::DropCharacterDistribution()
{
    TC_LOG_INFO("playerbots.migration", "Dropping character distribution system...");
    // Placeholder for future rollback
    return true;
}

// ============================================================================
// File-based Migration Discovery (TrinityCore Pattern)
// ============================================================================

std::vector<PlayerbotMigrationMgr::MigrationFile> PlayerbotMigrationMgr::DiscoverMigrationFiles()
{
    std::vector<MigrationFile> migrations;

    try
    {
        std::string migrationPath = GetMigrationPath();
        std::filesystem::path migrationDir(migrationPath);

        // Check if migration directory exists
        if (!std::filesystem::exists(migrationDir) || !std::filesystem::is_directory(migrationDir))
        {
            TC_LOG_WARN("playerbots.migration", "Migration directory {} does not exist", migrationPath);
            return migrations;
        }

        // Scan directory for .sql files
        for (auto const& entry : std::filesystem::directory_iterator(migrationDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".sql")
            {
                std::string filename = entry.path().filename().string();
                std::string version = ExtractVersionFromFilename(filename);

                if (version.empty())
                {
                    TC_LOG_WARN("playerbots.migration", "Skipping file {} - could not extract version number", filename);
                    continue;
                }

                MigrationFile migration;
                migration.filename = filename;
                migration.fullPath = entry.path().string();
                migration.version = version;
                migration.description = ExtractDescriptionFromFilename(filename);
                migration.isApplied = IsMigrationApplied(version);

                migrations.push_back(migration);

                TC_LOG_DEBUG("playerbots.migration", "Discovered migration file: {} (version: {}, applied: {})",
                    filename, version, migration.isApplied ? "yes" : "no");
            }
        }

        // Sort by version number for proper order
        std::sort(migrations.begin(), migrations.end(), [this](MigrationFile const& a, MigrationFile const& b) {
            return CompareVersions(a.version, b.version) < 0;
        });

        TC_LOG_INFO("playerbots.migration", "Discovered {} migration files in {}", migrations.size(), migrationPath);
    }
    catch (std::filesystem::filesystem_error const& ex)
    {
        TC_LOG_ERROR("playerbots.migration", "Filesystem error while discovering migration files: {}", ex.what());
    }

    return migrations;
}

std::string PlayerbotMigrationMgr::ExtractVersionFromFilename(std::string const& filename)
{
    // Pattern: XXX_description.sql (e.g., "001_initial_schema.sql" -> "001")
    std::regex versionRegex(R"(^(\d{3})_.*\.sql$)");
    std::smatch match;

    if (std::regex_match(filename, match, versionRegex))
    {
        return match[1].str();
    }

    return "";
}

std::string PlayerbotMigrationMgr::ExtractDescriptionFromFilename(std::string const& filename)
{
    // Pattern: XXX_description.sql (e.g., "001_initial_schema.sql" -> "Initial schema")
    std::regex descRegex(R"(^\d{3}_(.*)\.sql$)");
    std::smatch match;

    if (std::regex_match(filename, match, descRegex))
    {
        std::string desc = match[1].str();
        // Replace underscores with spaces and capitalize
        std::replace(desc.begin(), desc.end(), '_', ' ');
        if (!desc.empty())
            desc[0] = std::toupper(desc[0]);
        return desc;
    }

    return "Unknown migration";
}

bool PlayerbotMigrationMgr::ApplyMigrationFile(MigrationFile const& migration)
{
    TC_LOG_INFO("playerbots.migration", "Applying migration file: {} ({})", migration.filename, migration.description);

    auto startTime = std::chrono::high_resolution_clock::now();

    // Execute the SQL file
    if (!ExecuteSQLFile(migration.fullPath))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to execute migration file: {}", migration.fullPath);
        return false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Calculate file checksum for integrity
    std::string checksum = CalculateFileChecksum(migration.fullPath);

    // Record successful migration
    if (!RecordMigration(migration.version, migration.description, static_cast<uint32>(duration.count()), checksum))
    {
        TC_LOG_ERROR("playerbots.migration", "Failed to record migration {} in database", migration.version);
        return false;
    }

    TC_LOG_INFO("playerbots.migration", "Migration {} applied successfully in {}ms",
        migration.version, duration.count());

    return true;
}