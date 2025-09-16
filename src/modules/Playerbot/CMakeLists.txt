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

# Collect all source files
CollectSourceFiles(
    ${CMAKE_CURRENT_SOURCE_DIR}
    PLAYERBOT_SOURCES
    # Exclude test files from main build
    ${CMAKE_CURRENT_SOURCE_DIR}/Tests
)

# Group sources for IDE organization
GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

# Collect include directories
CollectIncludeDirectories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    PLAYERBOT_INCLUDES
)

# Create the playerbot library
add_library(playerbot ${PLAYERBOT_SOURCES})

# Set target properties
target_compile_features(playerbot PRIVATE cxx_std_20)

# Include directories
target_include_directories(playerbot
    PUBLIC
        ${PLAYERBOT_INCLUDES}
    PRIVATE
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
target_compile_definitions(playerbot PRIVATE PLAYERBOT_ENABLED)

# Install configuration file
if(CONF_DIR)
    install(FILES conf/playerbots.conf.dist DESTINATION ${CONF_DIR})
endif()

# Add to game library dependencies
if(TARGET game)
    target_link_libraries(game PRIVATE playerbot)
    target_compile_definitions(game PRIVATE WITH_PLAYERBOTS)
endif()

# Unit tests (optional)
if(BUILD_TESTING)
    add_subdirectory(Tests)
endif()