/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotDatabase.h"
#include "MySQLPreparedStatement.h"

PlayerbotDatabaseWorkerPool PlayerbotDatabase;

PlayerbotDatabaseConnection::PlayerbotDatabaseConnection(MySQLConnectionInfo& connInfo) : MySQLConnection(connInfo)
{
}

PlayerbotDatabaseConnection::PlayerbotDatabaseConnection(ProducerConsumerQueue<SQLOperation<PlayerbotDatabaseConnection>*>* q, MySQLConnectionInfo& connInfo) : MySQLConnection(q, connInfo)
{
}

PlayerbotDatabaseConnection::~PlayerbotDatabaseConnection()
{
}

void PlayerbotDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_PLAYERBOT_DATABASE_STATEMENTS);

    // Account management statements
    PrepareStatement(PLAYERBOT_SEL_BOT_ACCOUNTS,
        "SELECT account_id, is_bot, is_active, character_count, last_login FROM playerbot_accounts WHERE is_bot = 1", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_BOT_ACCOUNT_BY_ID,
        "SELECT account_id, is_bot, is_active, character_count, last_login FROM playerbot_accounts WHERE account_id = ?", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_INS_BOT_ACCOUNT,
        "INSERT INTO playerbot_accounts (account_id, is_bot, is_active, character_count) VALUES (?, 1, 1, 0)", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_UPD_BOT_ACCOUNT_ACTIVE,
        "UPDATE playerbot_accounts SET is_active = ? WHERE account_id = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_UPD_BOT_ACCOUNT_CHARACTER_COUNT,
        "UPDATE playerbot_accounts SET character_count = ? WHERE account_id = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_DEL_BOT_ACCOUNT,
        "DELETE FROM playerbot_accounts WHERE account_id = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_SEL_ACCOUNT_CHARACTER_COUNT,
        "SELECT COUNT(*) FROM characters WHERE account = ?", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_ACCOUNTS_EXCEEDING_LIMIT,
        "SELECT account_id, character_count FROM playerbot_accounts WHERE is_bot = 1 AND character_count > ?", 
        CONNECTION_SYNCH);

    // Name management statements
    PrepareStatement(PLAYERBOT_SEL_ALL_NAMES,
        "SELECT name_id, name, gender FROM playerbots_names", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_AVAILABLE_NAMES_BY_GENDER,
        "SELECT pn.name_id, pn.name FROM playerbots_names pn "
        "LEFT JOIN playerbots_names_used pnu ON pn.name_id = pnu.name_id "
        "WHERE pn.gender = ? AND pnu.name_id IS NULL", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_USED_NAMES,
        "SELECT name_id, character_guid FROM playerbots_names_used", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_INS_NAME_USED,
        "INSERT INTO playerbots_names_used (name_id, character_guid) VALUES (?, ?)", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_DEL_NAME_USED,
        "DELETE FROM playerbots_names_used WHERE name_id = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_DEL_NAME_USED_BY_GUID,
        "DELETE FROM playerbots_names_used WHERE character_guid = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_SEL_NAME_BY_GUID,
        "SELECT pn.name FROM playerbots_names pn "
        "INNER JOIN playerbots_names_used pnu ON pn.name_id = pnu.name_id "
        "WHERE pnu.character_guid = ?", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_IS_NAME_AVAILABLE,
        "SELECT 1 FROM playerbots_names pn "
        "LEFT JOIN playerbots_names_used pnu ON pn.name_id = pnu.name_id "
        "WHERE pn.name = ? AND pnu.name_id IS NULL", 
        CONNECTION_SYNCH);

    // Character management statements
    PrepareStatement(PLAYERBOT_SEL_BOT_CHARACTERS,
        "SELECT guid, account_id, name, race, class, level, role, last_login FROM playerbot_characters", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_BOT_CHARACTER_BY_GUID,
        "SELECT guid, account_id, name, race, class, level, role, last_login FROM playerbot_characters WHERE guid = ?", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_INS_BOT_CHARACTER,
        "INSERT INTO playerbot_characters (guid, account_id, name, race, class, level, role) VALUES (?, ?, ?, ?, ?, ?, ?)", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_UPD_BOT_CHARACTER,
        "UPDATE playerbot_characters SET level = ?, role = ?, last_login = NOW() WHERE guid = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_DEL_BOT_CHARACTER,
        "DELETE FROM playerbot_characters WHERE guid = ?", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_SEL_BOT_CHARACTERS_BY_ACCOUNT,
        "SELECT guid, name, race, class, level, role FROM playerbot_characters WHERE account_id = ? ORDER BY level DESC", 
        CONNECTION_SYNCH);

    // Character distribution statements
    PrepareStatement(PLAYERBOT_SEL_RACE_DISTRIBUTION,
        "SELECT race, weight, male_ratio, faction FROM playerbot_race_distribution WHERE enabled = 1", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_CLASS_DISTRIBUTION,
        "SELECT class, weight, tank_capable, healer_capable FROM playerbot_class_distribution WHERE enabled = 1", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_SEL_RACE_CLASS_MULTIPLIERS,
        "SELECT race, class, multiplier FROM playerbot_race_class_multipliers", 
        CONNECTION_SYNCH);

    // Performance metrics statements
    PrepareStatement(PLAYERBOT_INS_PERFORMANCE_METRIC,
        "INSERT INTO playerbot_performance_metrics (metric_name, value, recorded_at) VALUES (?, ?, NOW())", 
        CONNECTION_ASYNC);
    
    PrepareStatement(PLAYERBOT_SEL_PERFORMANCE_METRICS,
        "SELECT metric_name, AVG(value) as avg_value, COUNT(*) as sample_count "
        "FROM playerbot_performance_metrics "
        "WHERE recorded_at >= DATE_SUB(NOW(), INTERVAL ? HOUR) "
        "GROUP BY metric_name", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_DEL_OLD_PERFORMANCE_METRICS,
        "DELETE FROM playerbot_performance_metrics WHERE recorded_at < DATE_SUB(NOW(), INTERVAL ? DAY)", 
        CONNECTION_ASYNC);

    // Configuration statements
    PrepareStatement(PLAYERBOT_SEL_CONFIG,
        "SELECT `key`, `value` FROM playerbot_config", 
        CONNECTION_SYNCH);
    
    PrepareStatement(PLAYERBOT_REP_CONFIG,
        "REPLACE INTO playerbot_config (`key`, `value`, `description`) VALUES (?, ?, ?)", 
        CONNECTION_ASYNC);
}
