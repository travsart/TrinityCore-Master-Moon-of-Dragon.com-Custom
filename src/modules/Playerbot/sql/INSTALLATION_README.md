# TrinityCore Playerbot Module - Database Installation Guide

## Overview

This directory contains the consolidated SQL installation files for the TrinityCore Playerbot module. The database schema and initial data have been consolidated from 65+ separate SQL files into 2 installation-ready files.

## Installation Files

| File | Purpose | Size | Description |
|------|---------|------|-------------|
| `consolidated_playerbot_schema.sql` | Database Schema | ~25KB | All table definitions, indexes, and stored procedures |
| `consolidated_playerbot_data.sql` | Initial Data | ~45KB | Seed data for activity patterns, zones, names, and distributions |

## Prerequisites

- MySQL 8.0+ or MariaDB 10.5+
- TrinityCore `playerbot-dev` branch compiled with `BUILD_PLAYERBOT=1`
- Database user with CREATE, INSERT, UPDATE, DELETE privileges
- Recommended: Separate `playerbot` database for isolation

## Installation Steps

### Step 1: Create Database (Recommended)

```sql
-- Create dedicated playerbot database
CREATE DATABASE `playerbot` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Create playerbot user (optional but recommended)
CREATE USER 'playerbotuser'@'localhost' IDENTIFIED BY 'secure_password_here';
GRANT ALL PRIVILEGES ON `playerbot`.* TO 'playerbotuser'@'localhost';
FLUSH PRIVILEGES;
```

### Step 2: Install Schema

```bash
# Method 1: MySQL command line
mysql -u playerbotuser -p playerbot < consolidated_playerbot_schema.sql

# Method 2: MySQL Workbench or phpMyAdmin
# Import the consolidated_playerbot_schema.sql file through the GUI
```

### Step 3: Install Initial Data

```bash
# Method 1: MySQL command line
mysql -u playerbotuser -p playerbot < consolidated_playerbot_data.sql

# Method 2: MySQL Workbench or phpMyAdmin
# Import the consolidated_playerbot_data.sql file through the GUI
```

### Step 4: Verify Installation

```sql
-- Connect to playerbot database
USE playerbot;

-- Verify all tables were created
SHOW TABLES;

-- Should show 12 tables:
-- playerbot_accounts
-- playerbot_activity_patterns
-- playerbot_character_distribution
-- playerbot_lifecycle_events
-- playerbot_migrations
-- playerbot_schedules
-- playerbot_spawn_log
-- playerbot_zone_populations
-- playerbots_class_popularity
-- playerbots_gender_distribution
-- playerbots_names
-- playerbots_names_used
-- playerbots_race_class_distribution
-- playerbots_race_class_gender

-- Verify data was loaded
SELECT 'Activity Patterns' as Component, COUNT(*) as Count FROM playerbot_activity_patterns
UNION ALL
SELECT 'Zone Populations', COUNT(*) FROM playerbot_zone_populations
UNION ALL
SELECT 'Available Names', COUNT(*) FROM playerbots_names
UNION ALL
SELECT 'Class Popularity', COUNT(*) FROM playerbots_class_popularity;
```

Expected results:
- Activity Patterns: 16
- Zone Populations: 47
- Available Names: 100
- Class Popularity: 10

## Configuration

### Update worldserver.conf

Add database connection for playerbot:

```ini
###################################################################################################
# PLAYERBOT DATABASE SETTINGS
###################################################################################################

# Playerbot Database connection settings
# Use same format as other database connections
PlayerbotDatabaseInfo = "localhost;3306;playerbotuser;secure_password_here;playerbot"

# Connection pooling for playerbot database
PlayerbotDatabase.WorkerThreads = 2
PlayerbotDatabase.SynchThreads = 1
```

### Update playerbots.conf

The module will automatically detect the playerbot database. Key settings:

```ini
# Enable the playerbot module
Playerbot.Enable = 1

# Name database table (should match our installation)
Playerbot.NameDatabase = "playerbots_names"

# Maximum number of random bots
Playerbot.MaxRandomBots = 100

# Activity patterns (matches our data)
Playerbot.DefaultActivityPattern = "casual_medium"
```

## Database Schema Overview

### Core Tables

| Table | Purpose | Records |
|-------|---------|---------|
| `playerbot_activity_patterns` | Bot behavior patterns | 16 default patterns |
| `playerbot_schedules` | Bot login/logout scheduling | Dynamic (per bot) |
| `playerbot_spawn_log` | Spawn event tracking | Dynamic (rotated) |
| `playerbot_zone_populations` | Zone population control | 47 configured zones |
| `playerbot_lifecycle_events` | General event logging | Dynamic (rotated) |

### Character Management

| Table | Purpose | Records |
|-------|---------|---------|
| `playerbots_names` | Available bot names | 100 sample names |
| `playerbots_names_used` | Names currently in use | Dynamic |
| `playerbots_class_popularity` | Class distribution weights | 10 classes |
| `playerbots_gender_distribution` | Gender ratios by race | 10 races |
| `playerbots_race_class_distribution` | Valid race-class combinations | 65 combinations |

### Account Management

| Table | Purpose | Records |
|-------|---------|---------|
| `playerbot_accounts` | Bot account tracking | Dynamic |
| `playerbot_character_distribution` | Character distribution per account | Dynamic |

## Maintenance

### Automatic Cleanup

The schema includes stored procedures for maintenance:

```sql
-- Clean old spawn logs (30+ days)
CALL sp_cleanup_spawn_logs();

-- Clean old lifecycle events (7+ days, except errors)
CALL sp_cleanup_lifecycle_events();

-- Update zone population counts
CALL sp_update_zone_populations();

-- Get system statistics
CALL sp_get_system_stats();
```

### Recommended Cron Job

```bash
# Daily cleanup at 3 AM
0 3 * * * mysql -u playerbotuser -p'password' playerbot -e "CALL sp_cleanup_spawn_logs(); CALL sp_cleanup_lifecycle_events();"

# Hourly population updates
0 * * * * mysql -u playerbotuser -p'password' playerbot -e "CALL sp_update_zone_populations();"
```

## Troubleshooting

### Common Issues

1. **Foreign Key Constraint Errors**
   ```sql
   -- Disable temporarily during installation
   SET FOREIGN_KEY_CHECKS = 0;
   -- Re-enable after installation
   SET FOREIGN_KEY_CHECKS = 1;
   ```

2. **Character Set Issues**
   ```sql
   -- Verify database charset
   SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA
   WHERE SCHEMA_NAME = 'playerbot';
   ```

3. **Permission Issues**
   ```sql
   -- Verify user permissions
   SHOW GRANTS FOR 'playerbotuser'@'localhost';
   ```

### Validation Queries

```sql
-- Verify schema compatibility
SELECT
    TABLE_NAME,
    COLUMN_NAME,
    DATA_TYPE,
    IS_NULLABLE
FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA = 'playerbot'
AND TABLE_NAME LIKE 'playerbot%'
ORDER BY TABLE_NAME, ORDINAL_POSITION;

-- Check for any missing indexes
SELECT
    TABLE_NAME,
    INDEX_NAME,
    GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) as Columns
FROM INFORMATION_SCHEMA.STATISTICS
WHERE TABLE_SCHEMA = 'playerbot'
GROUP BY TABLE_NAME, INDEX_NAME
ORDER BY TABLE_NAME, INDEX_NAME;
```

## Performance Considerations

### Recommended Settings

```sql
-- For high-volume bot installations (500+ bots)
SET GLOBAL innodb_buffer_pool_size = 1073741824;  -- 1GB
SET GLOBAL query_cache_size = 67108864;           -- 64MB
SET GLOBAL max_connections = 200;
```

### Monitoring Queries

```sql
-- Monitor table sizes
SELECT
    TABLE_NAME,
    ROUND(((DATA_LENGTH + INDEX_LENGTH) / 1024 / 1024), 2) AS 'Size_MB',
    TABLE_ROWS
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'playerbot'
ORDER BY (DATA_LENGTH + INDEX_LENGTH) DESC;

-- Monitor recent activity
SELECT
    event_category,
    COUNT(*) as event_count,
    MAX(event_timestamp) as last_event
FROM playerbot_lifecycle_events
WHERE event_timestamp >= DATE_SUB(NOW(), INTERVAL 1 HOUR)
GROUP BY event_category
ORDER BY event_count DESC;
```

## Migration from Legacy Systems

If migrating from existing `mod-playerbots` installations:

1. **Backup existing data**
   ```bash
   mysqldump -u root -p characters playerbots_names > legacy_names_backup.sql
   ```

2. **Import legacy names** (if desired)
   ```sql
   -- Map legacy names to new structure
   INSERT INTO playerbots_names (name, gender, race_mask)
   SELECT name, gender, 0 as race_mask
   FROM legacy_characters_db.playerbots_names
   WHERE name NOT IN (SELECT name FROM playerbots_names);
   ```

## Support

For issues or questions:

1. Check TrinityCore playerbot module documentation
2. Verify all tables match the schema in `PlayerbotDatabaseStatements.cpp`
3. Check server logs for specific error messages
4. Ensure `BUILD_PLAYERBOT=1` was used during compilation

## File History

- **consolidated_playerbot_schema.sql**: Generated September 19, 2025
- **consolidated_playerbot_data.sql**: Generated September 19, 2025
- **Source**: Consolidated from 65+ individual SQL files
- **Compatibility**: TrinityCore playerbot-dev branch (commit 53e4b8bf96)