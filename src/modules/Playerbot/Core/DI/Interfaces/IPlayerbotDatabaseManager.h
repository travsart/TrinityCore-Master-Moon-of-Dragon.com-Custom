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
#include "DatabaseEnvFwd.h"
#include <string>
#include <memory>

/**
 * @brief Interface for playerbot database operations
 *
 * Provides simplified database access for playerbot systems, managing
 * connection lifecycle and query execution without complex templates.
 */
class TC_DATABASE_API IPlayerbotDatabaseManager
{
public:
    virtual ~IPlayerbotDatabaseManager() = default;

    // Connection management
    virtual bool Initialize(std::string const& connectionInfo) = 0;
    virtual void Close() = 0;

    // Query operations
    virtual QueryResult Query(std::string const& sql) = 0;
    virtual bool Execute(std::string const& sql) = 0;

    // Status
    virtual bool IsConnected() const = 0;
    virtual bool ValidateSchema() = 0;
};
