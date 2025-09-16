/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PLAYERBOT_DATABASE_H
#define PLAYERBOT_DATABASE_H

#include "DatabaseWorkerPool.h"
#include "MySQLConnection.h"

enum PlayerbotDatabaseStatements : uint32
{
    /*  Naming convention:
        PLAYERBOT_[ACTION]_[DESCRIPTION]
        ACTION: INS (insert), SEL (select), UPD (update), DEL (delete), REP (replace)
    */

    // Account management
    PLAYERBOT_SEL_BOT_ACCOUNTS,
    PLAYERBOT_SEL_BOT_ACCOUNT_BY_ID,
    PLAYERBOT_INS_BOT_ACCOUNT,
    PLAYERBOT_UPD_BOT_ACCOUNT_ACTIVE,
    PLAYERBOT_UPD_BOT_ACCOUNT_CHARACTER_COUNT,
    PLAYERBOT_DEL_BOT_ACCOUNT,
    PLAYERBOT_SEL_ACCOUNT_CHARACTER_COUNT,
    PLAYERBOT_SEL_ACCOUNTS_EXCEEDING_LIMIT,
    
    // Name management
    PLAYERBOT_SEL_ALL_NAMES,
    PLAYERBOT_SEL_AVAILABLE_NAMES_BY_GENDER,
    PLAYERBOT_SEL_USED_NAMES,
    PLAYERBOT_INS_NAME_USED,
    PLAYERBOT_DEL_NAME_USED,
    PLAYERBOT_DEL_NAME_USED_BY_GUID,
    PLAYERBOT_SEL_NAME_BY_GUID,
    PLAYERBOT_SEL_IS_NAME_AVAILABLE,
    
    // Character management
    PLAYERBOT_SEL_BOT_CHARACTERS,
    PLAYERBOT_SEL_BOT_CHARACTER_BY_GUID,
    PLAYERBOT_INS_BOT_CHARACTER,
    PLAYERBOT_UPD_BOT_CHARACTER,
    PLAYERBOT_DEL_BOT_CHARACTER,
    PLAYERBOT_SEL_BOT_CHARACTERS_BY_ACCOUNT,
    
    // Character distribution
    PLAYERBOT_SEL_RACE_DISTRIBUTION,
    PLAYERBOT_SEL_CLASS_DISTRIBUTION,
    PLAYERBOT_SEL_RACE_CLASS_MULTIPLIERS,
    
    // Performance metrics
    PLAYERBOT_INS_PERFORMANCE_METRIC,
    PLAYERBOT_SEL_PERFORMANCE_METRICS,
    PLAYERBOT_DEL_OLD_PERFORMANCE_METRICS,
    
    // Configuration
    PLAYERBOT_SEL_CONFIG,
    PLAYERBOT_REP_CONFIG,
    
    MAX_PLAYERBOT_DATABASE_STATEMENTS
};

class TC_DATABASE_API PlayerbotDatabaseConnection : public MySQLConnection
{
public:
    typedef PlayerbotDatabaseStatements Statements;

    //- Constructor
    PlayerbotDatabaseConnection(MySQLConnectionInfo& connInfo, ConnectionFlags connectionFlags);
    ~PlayerbotDatabaseConnection();

    //- Loads database type specific prepared statements
    void DoPrepareStatements() override;
};

typedef DatabaseWorkerPool<PlayerbotDatabaseConnection> PlayerbotDatabaseWorkerPool;

enum PlayerbotDatabaseWorker
{
    PLAYERBOT_DB_ASYNC,
    PLAYERBOT_DB_SYNCH
};

extern TC_DATABASE_API PlayerbotDatabaseWorkerPool PlayerbotDatabase;

#endif // PLAYERBOT_DATABASE_H
