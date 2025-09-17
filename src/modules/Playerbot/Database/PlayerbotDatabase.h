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
#include "PlayerbotDatabaseStatements.h"

// Enum is defined in PlayerbotDatabaseStatements.h

class TC_DATABASE_API PlayerbotDatabaseConnection : public MySQLConnection
{
public:
    using Statements = PlayerbotDatabaseStatements;

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
