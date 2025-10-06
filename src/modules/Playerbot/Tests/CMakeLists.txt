# Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.

# Playerbot Unit Tests
cmake_minimum_required(VERSION 3.24)

# Only build tests if explicitly requested
option(BUILD_PLAYERBOT_TESTS "Build Playerbot unit tests" OFF)

if(BUILD_PLAYERBOT_TESTS)
    # Find required test frameworks
    find_package(GTest REQUIRED)
    find_package(GMock REQUIRED)

    # Test executable
    set(PLAYERBOT_TEST_SOURCES
        BotSpawnOrchestratorTest.cpp
        BotPerformanceMonitorTest.cpp
        BotSpawnEventBusTest.cpp
        # Group functionality test suite
        TestUtilities.cpp
        GroupFunctionalityTests.cpp
        StressAndEdgeCaseTests.cpp
        PerformanceValidator.cpp
        AutomatedTestRunner.cpp
        ProductionValidationDemo.cpp
        # AI component tests
        BehaviorManagerTest.cpp
        # Future test files will be added here
    )

    # Create test executable
    add_executable(playerbot_tests ${PLAYERBOT_TEST_SOURCES})

    # Include directories
    target_include_directories(playerbot_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/modules/Playerbot
        ${CMAKE_SOURCE_DIR}/src/server/shared
        ${CMAKE_SOURCE_DIR}/src/server/database
        ${CMAKE_SOURCE_DIR}/src/server/game
        ${CMAKE_SOURCE_DIR}/src/server/worldserver
        ${CMAKE_SOURCE_DIR}/src/common
        ${GTEST_INCLUDE_DIRS}
        ${GMOCK_INCLUDE_DIRS}
    )

    # Link libraries
    target_link_libraries(playerbot_tests
        playerbot          # The main playerbot module
        shared             # TrinityCore shared components
        game               # TrinityCore game components
        ${GTEST_LIBRARIES}
        ${GMOCK_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
    )

    # Compiler definitions
    target_compile_definitions(playerbot_tests PRIVATE
        TRINITY_API_USE_DYNAMIC_LINKING
        PLAYERBOT_TESTS_ENABLED
    )

    # Set C++ standard
    set_target_properties(playerbot_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Add test discovery for CTest
    if(TARGET gtest_discover_tests)
        gtest_discover_tests(playerbot_tests)
    else()
        add_test(NAME PlayerBotTests COMMAND playerbot_tests)
    endif()

    # Custom test target for convenience
    add_custom_target(run_playerbot_tests
        COMMAND playerbot_tests
        DEPENDS playerbot_tests
        COMMENT "Running Playerbot unit tests"
    )

    # Performance test target (runs tests with performance profiling)
    add_custom_target(playerbot_perf_tests
        COMMAND playerbot_tests --gtest_filter="*Performance*:*Stress*:*HighThroughput*"
        DEPENDS playerbot_tests
        COMMENT "Running Playerbot performance tests"
    )

    # Test coverage target (if coverage tools are available)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(GCOV_PATH gcov)
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)

        if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
            target_compile_options(playerbot_tests PRIVATE --coverage)
            target_link_libraries(playerbot_tests --coverage)

            add_custom_target(playerbot_coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory coverage
                COMMAND ${LCOV_PATH} --directory . --zerocounters
                COMMAND $<TARGET_FILE:playerbot_tests>
                COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage/playerbot.info
                COMMAND ${LCOV_PATH} --remove coverage/playerbot.info '/usr/*' '*/tests/*' '*/gtest/*' --output-file coverage/playerbot_filtered.info
                COMMAND ${GENHTML_PATH} coverage/playerbot_filtered.info --output-directory coverage/html
                DEPENDS playerbot_tests
                COMMENT "Generating test coverage report"
            )
        endif()
    endif()

    # Sanitizer builds for enhanced testing
    option(PLAYERBOT_TESTS_ASAN "Build tests with AddressSanitizer" OFF)
    option(PLAYERBOT_TESTS_TSAN "Build tests with ThreadSanitizer" OFF)
    option(PLAYERBOT_TESTS_UBSAN "Build tests with UndefinedBehaviorSanitizer" OFF)

    if(PLAYERBOT_TESTS_ASAN)
        target_compile_options(playerbot_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(playerbot_tests PRIVATE -fsanitize=address)
    endif()

    if(PLAYERBOT_TESTS_TSAN)
        target_compile_options(playerbot_tests PRIVATE -fsanitize=thread)
        target_link_options(playerbot_tests PRIVATE -fsanitize=thread)
    endif()

    if(PLAYERBOT_TESTS_UBSAN)
        target_compile_options(playerbot_tests PRIVATE -fsanitize=undefined)
        target_link_options(playerbot_tests PRIVATE -fsanitize=undefined)
    endif()

    # Benchmark integration (if Google Benchmark is available)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        set(PLAYERBOT_BENCHMARK_SOURCES
            BenchmarkSpawnPerformance.cpp
            # Future benchmark files
        )

        add_executable(playerbot_benchmarks ${PLAYERBOT_BENCHMARK_SOURCES})

        target_include_directories(playerbot_benchmarks PRIVATE
            ${CMAKE_SOURCE_DIR}/src/modules/Playerbot
            ${CMAKE_SOURCE_DIR}/src/server/shared
            ${CMAKE_SOURCE_DIR}/src/server/game
        )

        target_link_libraries(playerbot_benchmarks
            playerbot
            shared
            game
            benchmark::benchmark
        )

        add_custom_target(run_playerbot_benchmarks
            COMMAND playerbot_benchmarks
            DEPENDS playerbot_benchmarks
            COMMENT "Running Playerbot performance benchmarks"
        )
    endif()

    # Test data setup
    set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/test_data)
    file(MAKE_DIRECTORY ${TEST_DATA_DIR})

    # Copy test configuration files
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/test_config.conf.in
        ${TEST_DATA_DIR}/test_config.conf
        @ONLY
    )

    # Test environment setup
    set_tests_properties(PlayerBotTests PROPERTIES
        ENVIRONMENT "PLAYERBOT_TEST_DATA_DIR=${TEST_DATA_DIR}"
        TIMEOUT 300  # 5 minutes timeout for all tests
    )

    # Install test targets (for development environments)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        install(TARGETS playerbot_tests
            RUNTIME DESTINATION bin/tests
            COMPONENT tests
        )

        install(DIRECTORY ${TEST_DATA_DIR}
            DESTINATION bin/tests/data
            COMPONENT tests
        )
    endif()

    message(STATUS "Playerbot tests configured:")
    message(STATUS "  - Unit tests: playerbot_tests")
    message(STATUS "  - Performance tests: playerbot_perf_tests")
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        message(STATUS "  - Coverage reports: playerbot_coverage")
    endif()
    if(benchmark_FOUND)
        message(STATUS "  - Benchmarks: playerbot_benchmarks")
    endif()

else()
    message(STATUS "Playerbot tests disabled. Use -DBUILD_PLAYERBOT_TESTS=ON to enable.")
endif()

# Test-specific compiler warnings
if(BUILD_PLAYERBOT_TESTS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(playerbot_tests PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter  # GTest/GMock generate unused parameters
            -Wno-sign-compare      # GTest comparisons sometimes trigger this
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(playerbot_tests PRIVATE
            /W4
            /wd4100  # Unused parameter warning (common in test frameworks)
            /wd4389  # Signed/unsigned comparison (GTest)
        )
    endif()
endif()