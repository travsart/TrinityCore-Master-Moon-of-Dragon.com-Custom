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
# Note: Dependencies are validated in FindPlayerbotDependencies.cmake
# The validation script will fail fast if any critical dependencies are missing
message(STATUS "Enterprise dependencies validated by FindPlayerbotDependencies.cmake")

message(STATUS "✅ All enterprise dependencies validated")
message(STATUS "Playerbot module: ENABLED with enterprise optimizations")

# Set source files - Enterprise Phase 1 Implementation
set(PLAYERBOT_SOURCES
    # Module entry point
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h

    # Configuration system (basic)
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
    # Temporarily disabled: DependencyValidator (needs MySQL headers fix)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Config/DependencyValidator.h

    # Session management (essential for bot functionality)
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h

    # Database pool (basic implementation)
    # Temporarily disabled: BotDatabasePool (needs API updates)
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/Database/BotDatabasePool.h

    # AI system (AI Framework implementation)
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h

    # AI Strategy System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h

    # AI Action System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h

    # AI Trigger System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h

    # AI Value System
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h

    # Group Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h

    # Movement System
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h

    # Phase 2: Account Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h

    # Phase 2: Character Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h

    # Phase 2: Bot Lifecycle Management System
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h

    # Phase 2: Refactored Architecture Components
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h

    # Phase 2: Enhanced Session Management
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h

    # AI Combat System - Group Combat Coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LeaderFollowBehavior.h

    # Phase 2: Interface Definitions (header-only)
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h

    # Phase 2: Adapter Pattern Implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h

    # Database systems
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h

    # Script integration
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h
)

# Group sources for IDE organization
source_group("" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerbotModuleAdapter.h
)

source_group("Config" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/PlayerbotLog.h
)


source_group("Database" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotCharacterDBInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseConnection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotResultSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotDatabaseStatements.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Database/PlayerbotMigrationMgr.h
)

source_group("AI" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/BotAI.h
)

source_group("AI\\Strategy" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/Strategy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy/LeaderFollowBehavior.h
)

source_group("AI\\Actions" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/Action.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/CommonActions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/TargetAssistAction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions/SpellInterruptAction.h
)

source_group("AI\\Triggers" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers/Trigger.h
)

source_group("AI\\Combat" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/GroupCombatTrigger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/RoleBasedCombatPositioning.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/MechanicAwareness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/InterruptAwareness.h
    # ThreatManager - resolved naming conflict by renaming to BotThreatManager
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/BotThreatManager.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatAbilities.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/ThreatCoordinator.h
# Phase 2 Day 14: Combat AI Integration    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Combat/CombatAIIntegrator.h    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/AI/EnhancedBotAI.h
)

source_group("AI\\Values" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values/Value.h
)

source_group("Group" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Group/GroupInvitationHandler.h
)

source_group("Movement" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Movement/LeaderFollowBehavior.h
)

source_group("Account" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Account/BotAccountMgr.h
)

source_group("Character" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotNameMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCharacterDistribution.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Character/BotCustomizationGenerator.h
)

source_group("Lifecycle" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotLifecycleMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnOrchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotResourcePool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotPerformanceMonitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle/BotSpawnEventBus.h
)

source_group("Session" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotWorldSessionMgr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session/BotSessionFactory.h
)

source_group("Interfaces" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSpawner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces/IBotPerformance.h
)

source_group("Adapters" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Adapters/BotSpawnerAdapter.h
)

source_group("Scripts" FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scripts/PlayerbotWorldScript.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Strategy
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Actions
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Triggers
        ${CMAKE_CURRENT_SOURCE_DIR}/AI/Values
        ${CMAKE_CURRENT_SOURCE_DIR}/Interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/Adapters
        ${CMAKE_SOURCE_DIR}/src/modules/Common
        ${MYSQL_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Account
        ${CMAKE_CURRENT_SOURCE_DIR}/Character
        ${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle
        ${CMAKE_CURRENT_SOURCE_DIR}/Scripts
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
        # Shared module libraries
        modules-common
    PRIVATE
        # Enterprise dependencies
        ${PLAYERBOT_LIBRARIES}
        playerbot-dependencies
        # MySQL libraries for database connection
        ${MYSQL_LIBRARY}
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
        BUILD_PLAYERBOT=1
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

# Enterprise optimization flags - only for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Configuring enterprise performance optimizations")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:-O3>                    # Maximum optimization
            $<$<CONFIG:Release>:-march=native>          # Target current CPU
            $<$<CONFIG:Release>:-mtune=native>          # Tune for current CPU
            $<$<CONFIG:Release>:-flto>                  # Link-time optimization
            $<$<CONFIG:Release>:-ffast-math>            # Fast math for SIMD
            $<$<CONFIG:Release>:-funroll-loops>         # Loop unrolling
            $<$<CONFIG:Release>:-ftree-vectorize>       # Auto-vectorization
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(playerbot PRIVATE
            $<$<CONFIG:Release>:/O2>                    # Maximum optimization (Release only)
            $<$<CONFIG:Release>:/GL>                    # Whole program optimization (Release only)
            $<$<CONFIG:Release>:/fp:fast>               # Fast floating point (Release only)
            $<$<CONFIG:Release>:/arch:AVX2>             # Enable AVX2 if available (Release only)
        )
        # Apply link-time optimizations only for Release builds
        target_link_options(playerbot PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()
endif()

# Install configuration file
if(CONF_DIR)
    install(FILES conf/playerbots.conf.dist DESTINATION ${CONF_DIR}/modules)
endif()

# Install SQL migration files and structure
if(CONF_DIR)
    # Create sql/migrations directory in installation
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql)
    install(DIRECTORY DESTINATION ${CONF_DIR}/sql/migrations)

    # Install all migration files
    install(FILES
        sql/migrations/001_initial_schema.sql
        sql/migrations/002_account_management.sql
        sql/migrations/003_lifecycle_management.sql
        sql/migrations/004_character_distribution.sql
        sql/migrations/005_initial_data.sql
        sql/migrations/006_bot_names.sql
        DESTINATION ${CONF_DIR}/sql/migrations
    )

    message(STATUS "Playerbot migration files will be installed to: ${CONF_DIR}/sql/migrations/")
endif()

# Legacy SQL installation (deprecated - keeping for compatibility)
if(SQL_DIR)
    install(DIRECTORY sql/ DESTINATION ${SQL_DIR}/playerbot)
endif()

# Add to game library dependencies
if(TARGET game)
    target_link_libraries(game PRIVATE playerbot)
    target_compile_definitions(game PRIVATE
        BUILD_PLAYERBOT=1
    )
endif()

# Enterprise dependency verification test (temporarily disabled)
# add_executable(playerbot-dependency-test
#     Config/DependencyValidator.cpp
#     Config/DependencyValidator.h
# )
#
# target_link_libraries(playerbot-dependency-test
#     PRIVATE
#         playerbot-dependencies
#         shared  # For Trinity logging
# )
#
# target_include_directories(playerbot-dependency-test
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/Config
# )
#
# # Run dependency validation as part of build
# add_custom_command(
#     TARGET playerbot-dependency-test POST_BUILD
#     COMMAND playerbot-dependency-test
#     COMMENT "Validating Playerbot enterprise dependencies"
#     VERBATIM
# )

# Unit tests (temporarily disabled)
# if(BUILD_TESTING)
#     # Performance benchmarks
#     add_executable(playerbot-benchmark
#         Tests/BenchmarkTests.cpp
#         Session/BotSession.cpp
#         Session/BotSessionMgr.cpp
#     )
#
#     target_link_libraries(playerbot-benchmark
#         PRIVATE
#             playerbot-dependencies
#             shared
#     )
#
#     target_include_directories(playerbot-benchmark
#         PRIVATE
#             ${CMAKE_CURRENT_SOURCE_DIR}
#     )
#
#     set_target_properties(playerbot-benchmark PROPERTIES
#         FOLDER "tests"
#         OUTPUT_NAME "playerbot-benchmark"
#     )
# endif()

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
