# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Playerbot Module CMake Configuration
# This module provides optional AI-controlled player bots for TrinityCore

if(NOT BUILD_PLAYERBOT)
    message(STATUS "Playerbot module: DISABLED (use -DBUILD_PLAYERBOT=1 to enable)")
    return()
endif()

message(STATUS "Playerbot module: ENABLED")

# Set source files
set(PLAYERBOT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
)

# Group sources for IDE organization
source_group("" FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
)

source_group("Config" FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
)

source_group("Account" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
)

source_group("Character" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
)

source_group("Database" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
)

# Create the playerbot library
add_library(playerbot ${PLAYERBOT_SOURCES})

# Set target properties
target_compile_features(playerbot PRIVATE cxx_std_20)

# Include directories
target_include_directories(playerbot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Config
        ${CMAKE_CURRENT_SOURCE_DIR}/Account
        ${CMAKE_CURRENT_SOURCE_DIR}/Character
        ${CMAKE_CURRENT_SOURCE_DIR}/Database
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Link against TrinityCore libraries
target_link_libraries(playerbot
    PRIVATE
        game-interface
        shared
        database
        common
)

# Add preprocessor definition
target_compile_definitions(playerbot 
    PRIVATE 
        PLAYERBOT_ENABLED
        TRINITY_API_EXPORT_PLAYERBOT
)

# Install configuration file
if(CONF_DIR)
    install(FILES conf/playerbots.conf.dist DESTINATION ${CONF_DIR}/modules)
endif()

# Install SQL files
if(SQL_DIR)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/sql/playerbot/ DESTINATION ${SQL_DIR}/playerbot)
endif()

# Add to game library dependencies
if(TARGET game)
    target_link_libraries(game PRIVATE playerbot)
    target_compile_definitions(game PRIVATE WITH_PLAYERBOTS)
endif()

# Unit tests (optional)
if(BUILD_TESTING)
    # add_subdirectory(Tests)
endif()

message(STATUS "  -> Playerbot module source files: ${PLAYERBOT_SOURCES}")
