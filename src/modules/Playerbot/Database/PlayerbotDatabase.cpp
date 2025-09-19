/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotDatabase.h"
#include "Log.h"

PlayerbotDatabaseManager* PlayerbotDatabaseManager::instance()
{
    static PlayerbotDatabaseManager instance;
    return &instance;
}

bool PlayerbotDatabaseManager::Initialize(std::string const& connectionInfo)
{
    TC_LOG_DEBUG("module.playerbot.database", "PlayerbotDatabaseManager: Initializing connection");

    if (_connection && _connection->IsConnected())
    {
        TC_LOG_WARN("module.playerbot.database", "PlayerbotDatabaseManager: Already initialized");
        return true;
    }

    _connection = std::make_unique<PlayerbotDatabaseConnection>();

    if (!_connection->Initialize(connectionInfo))
    {
        TC_LOG_ERROR("module.playerbot.database", "PlayerbotDatabaseManager: Failed to initialize connection: {}",
                    _connection->GetLastError());
        _connection.reset();
        return false;
    }

    TC_LOG_INFO("module.playerbot.database", "PlayerbotDatabaseManager: Successfully initialized");
    return true;
}

void PlayerbotDatabaseManager::Close()
{
    if (_connection)
    {
        TC_LOG_DEBUG("module.playerbot.database", "PlayerbotDatabaseManager: Closing connection");
        _connection->Close();
        _connection.reset();
    }
}

QueryResult PlayerbotDatabaseManager::Query(std::string const& sql)
{
    if (!_connection)
    {
        TC_LOG_ERROR("module.playerbot.database", "PlayerbotDatabaseManager: No connection available for query");
        return nullptr;
    }

    return _connection->Query(sql);
}

bool PlayerbotDatabaseManager::Execute(std::string const& sql)
{
    if (!_connection)
    {
        TC_LOG_ERROR("module.playerbot.database", "PlayerbotDatabaseManager: No connection available for execute");
        return false;
    }

    return _connection->Execute(sql);
}

bool PlayerbotDatabaseManager::IsConnected() const
{
    return _connection && _connection->IsConnected();
}
