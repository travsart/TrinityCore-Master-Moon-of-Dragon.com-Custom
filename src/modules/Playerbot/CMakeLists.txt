# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# TrinityCore Playerbot Module - Enterprise Implementation
# High-performance AI-controlled player bots with Intel TBB, parallel hashmap, and boost optimizations

if(NOT BUILD_PLAYERBOT)
    message(STATUS "Playerbot module: DISABLED (use -DBUILD_PLAYERBOT=1 to enable)")
    return()
endif()

message(STATUS "=== Building TrinityCore Playerbot Enterprise Module ===")

# Include enterprise dependency validation
include(${CMAKE_SOURCE_DIR}/cmake/modules/FindPlayerbotDependencies.cmake)

# Verify all enterprise dependencies are available
if(NOT PLAYERBOT_TBB_FOUND OR NOT PLAYERBOT_PHMAP_FOUND OR
   NOT PLAYERBOT_BOOST_FOUND OR NOT PLAYERBOT_MYSQL_FOUND)
    message(FATAL_ERROR "Playerbot enterprise dependencies not satisfied. Install required dependencies:
    - Intel TBB 2021.5+
    - Parallel Hashmap 1.3.8+
    - Boost 1.74.0+
    - MySQL 8.0.33+
    See .claude/PHASE1/DEPENDENCY_VERIFICATION.md for installation instructions.")
endif()

message(STATUS "✅ All enterprise dependencies validated")
message(STATUS "Playerbot module: ENABLED with enterprise optimizations")

# Set source files - Enterprise Phase 1 Implementation
set(PLAYERBOT_SOURCES
    # Module entry point
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h

    # Configuration system with dependency validation
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.h

    # High-performance session management with Intel TBB
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h

    # Async database pool with boost::asio
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.h

    # AI system (minimal for Phase 1)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h

    # Legacy components (Phase 2 migration targets)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.h
)

source_group("Session" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
)

source_group("Database" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
)

source_group("AI" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
)

source_group("Account" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
)

source_group("Character" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
)

# Create the playerbot library
add_library(playerbot ${PLAYERBOT_SOURCES})

# Set target properties with enterprise C++20 standards
set_target_properties(playerbot PROPERTIES
    OUTPUT_NAME "playerbot"
    FOLDER "modules"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories with enterprise dependencies
target_include_directories(playerbot
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Config
        ${CMAKE_CURRENT_SOURCE_DIR}/Session
        ${CMAKE_CURRENT_SOURCE_DIR}/Database
        ${CMAKE_CURRENT_SOURCE_DIR}/AI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Account
        ${CMAKE_CURRENT_SOURCE_DIR}/Character
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PLAYERBOT_INCLUDE_DIRS}
)

# Link against TrinityCore and enterprise dependencies
target_link_libraries(playerbot
    PUBLIC
        # TrinityCore core libraries
        game-interface
        shared
        database
        common
    PRIVATE
        # Enterprise dependencies
        ${PLAYERBOT_LIBRARIES}
        playerbot-dependencies
)

# Platform-specific libraries for enterprise dependencies
if(WIN32)
    target_link_libraries(playerbot PRIVATE ws2_32 wsock32 mswsock)
elseif(UNIX)
    target_link_libraries(playerbot PRIVATE pthread dl)
endif()

# Enterprise preprocessor definitions
target_compile_definitions(playerbot
    PRIVATE
        PLAYERBOT_ENABLED=1
        TRINITY_API_EXPORT_PLAYERBOT
        $<$<CONFIG:Debug>:PLAYERBOT_DEBUG=1>
        $<$<CONFIG:Release>:PLAYERBOT_RELEASE=1>
)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(playerbot PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Enterprise optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling enterprise performance optimizations")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(playerbot PRIVATE
            -O3                    # Maximum optimization
            -march=native          # Target current CPU
            -mtune=native          # Tune for current CPU
            -flto                  # Link-time optimization
            -ffast-math            # Fast math for SIMD
            -funroll-loops         # Loop unrolling
            -ftree-vectorize       # Auto-vectorization
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(playerbot PRIVATE
            /O2                    # Maximum optimization
            /GL                    # Whole program optimization
            /fp:fast               # Fast floating point
            /arch:AVX2             # Enable AVX2 if available
        )
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    endif()
endif()

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

# Enterprise dependency verification test
add_executable(playerbot-dependency-test
    Config/DependencyValidator.cpp
    Config/DependencyValidator.h
)

target_link_libraries(playerbot-dependency-test
    PRIVATE
        playerbot-dependencies
        shared  # For Trinity logging
)

target_include_directories(playerbot-dependency-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Config
)

# Run dependency validation as part of build
add_custom_command(
    TARGET playerbot-dependency-test POST_BUILD
    COMMAND playerbot-dependency-test
    COMMENT "Validating Playerbot enterprise dependencies"
    VERBATIM
)

# Unit tests (optional)
if(BUILD_TESTING)
    # Performance benchmarks
    add_executable(playerbot-benchmark
        Tests/BenchmarkTests.cpp
        Session/BotSession.cpp
        Session/BotSessionMgr.cpp
    )

    target_link_libraries(playerbot-benchmark
        PRIVATE
            playerbot-dependencies
            shared
    )

    target_include_directories(playerbot-benchmark
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_target_properties(playerbot-benchmark PROPERTIES
        FOLDER "tests"
        OUTPUT_NAME "playerbot-benchmark"
    )
endif()

# Enterprise quality validation
message(STATUS "=== Playerbot Enterprise Configuration Summary ===")
message(STATUS "  -> Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  -> C++ Standard: C++20")
message(STATUS "  -> Intel TBB: ${PLAYERBOT_TBB_FOUND}")
message(STATUS "  -> Parallel Hashmap: ${PLAYERBOT_PHMAP_FOUND}")
message(STATUS "  -> Boost: ${PLAYERBOT_BOOST_FOUND}")
message(STATUS "  -> MySQL: ${PLAYERBOT_MYSQL_FOUND}")
message(STATUS "  -> Performance Optimizations: $<$<CONFIG:Release>:Enabled>")
message(STATUS "  -> Source files: ${PLAYERBOT_SOURCES}")
message(STATUS "🚀 TrinityCore Playerbot enterprise module ready for compilation")
