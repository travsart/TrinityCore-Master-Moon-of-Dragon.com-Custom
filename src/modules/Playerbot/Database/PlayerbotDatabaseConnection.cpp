/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "PlayerbotDatabaseConnection.h"
#include "PlayerbotResultSet.h"
#include "Log.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "MySQLWorkaround.h"
#include <sstream>

PlayerbotDatabaseConnection::PlayerbotDatabaseConnection()
    : _connected(false), _mysqlHandle(nullptr)
{
}

PlayerbotDatabaseConnection::~PlayerbotDatabaseConnection()
{
    Close();
}

bool PlayerbotDatabaseConnection::Initialize(std::string const& connectionInfo)
{
    if (_connected)
    {
        TC_LOG_WARN("module.playerbot.database", "PlayerbotDatabaseConnection: Already connected");
        return true;
    }

    // Parse connection string format: "hostname;port;username;password;database"
    std::vector<std::string> tokens;
    std::stringstream ss(connectionInfo);
    std::string token;

    while (std::getline(ss, token, ';'))
    {
        tokens.push_back(token);
    }

    if (tokens.size() != 5)
    {
        SetError("Invalid connection string format. Expected: hostname;port;username;password;database");
        return false;
    }

    std::string const& hostname = tokens[0];
    uint32 port = std::stoul(tokens[1]);
    std::string const& username = tokens[2];
    std::string const& password = tokens[3];
    std::string const& database = tokens[4];

    // Initialize MySQL handle
    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql)
    {
        SetError("Failed to initialize MySQL handle");
        return false;
    }

    // Set connection options
    bool reconnect = true;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

    // Connect to MySQL server
    if (!mysql_real_connect(mysql, hostname.c_str(), username.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, CLIENT_MULTI_STATEMENTS))
    {
        SetError(Trinity::StringFormat("Failed to connect to MySQL: {}", mysql_error(mysql)));
        mysql_close(mysql);
        return false;
    }

    _mysqlHandle = mysql;
    _connected = true;
    _lastError.clear();

    TC_LOG_INFO("module.playerbot.database", "PlayerbotDatabaseConnection: Connected to {}:{}/{}",
                hostname, port, database);

    return true;
}

void PlayerbotDatabaseConnection::Close()
{
    if (_mysqlHandle)
    {
        mysql_close(static_cast<MYSQL*>(_mysqlHandle));
        _mysqlHandle = nullptr;
    }
    _connected = false;
}

QueryResult PlayerbotDatabaseConnection::Query(std::string const& sql)
{
    if (!_connected || !_mysqlHandle)
    {
        SetError("Not connected to database");
        return nullptr;
    }

    MYSQL* mysql = static_cast<MYSQL*>(_mysqlHandle);

    if (mysql_query(mysql, sql.c_str()))
    {
        SetError(Trinity::StringFormat("Query failed: {}", mysql_error(mysql)));
        return nullptr;
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result)
    {
        // Check if it was a query that should return results
        if (mysql_field_count(mysql) > 0)
        {
            SetError(Trinity::StringFormat("Failed to store result: {}", mysql_error(mysql)));
            return nullptr;
        }
        else
        {
            // Query didn't return results (INSERT, UPDATE, DELETE, etc.)
            return nullptr;
        }
    }

    // Create a proper ResultSet from our MySQL result
    if (result)
    {
        // Create our custom PlayerbotResultSet that properly wraps the MySQL result
        try
        {
            auto resultSet = std::make_shared<PlayerbotResultSet>(result);
            TC_LOG_DEBUG("module.playerbot.database",
                "PlayerbotDatabaseConnection: Query returned {} rows", resultSet->GetRowCount());
            return resultSet;
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("module.playerbot.database",
                "PlayerbotDatabaseConnection: Failed to create ResultSet: {}", e.what());
            mysql_free_result(result);
            return nullptr;
        }
    }

    return nullptr;
}

bool PlayerbotDatabaseConnection::Execute(std::string const& sql)
{
    if (!_connected || !_mysqlHandle)
    {
        SetError("Not connected to database");
        return false;
    }

    MYSQL* mysql = static_cast<MYSQL*>(_mysqlHandle);

    if (mysql_query(mysql, sql.c_str()))
    {
        SetError(Trinity::StringFormat("Execute failed: {}", mysql_error(mysql)));
        return false;
    }

    // Clear any result set (for multi-statement support)
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
        mysql_free_result(result);

    return true;
}

void PlayerbotDatabaseConnection::SetError(std::string const& error)
{
    _lastError = error;
    TC_LOG_ERROR("module.playerbot.database", "PlayerbotDatabaseConnection: {}", error);
}