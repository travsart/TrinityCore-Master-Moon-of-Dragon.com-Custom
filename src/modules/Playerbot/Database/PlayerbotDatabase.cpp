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

PlayerbotDatabaseConnection::PlayerbotDatabaseConnection(MySQLConnectionInfo& connInfo, ConnectionFlags connectionFlags) : MySQLConnection(connInfo, connectionFlags)
{
}

PlayerbotDatabaseConnection::~PlayerbotDatabaseConnection()
{
}

void PlayerbotDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_PLAYERBOTDATABASE_STATEMENTS);

    // Use the statements defined in PlayerbotDatabaseStatements.h
    // This function will be called during database initialization
    // Individual prepared statements are loaded from the statements array

    // Prepare all statements from the PlayerbotDB namespace
    for (size_t i = 0; i < MAX_PLAYERBOTDATABASE_STATEMENTS; ++i)
    {
        PrepareStatement(static_cast<PlayerbotDatabaseStatements>(i),
                        PlayerbotDB::statements[i],
                        CONNECTION_SYNCH);
    }
}
