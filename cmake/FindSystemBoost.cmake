# FindSystemBoost.cmake - Force use of system Boost 1.78 instead of vcpkg
cmake_minimum_required(VERSION 3.24)

# Clear any vcpkg interference
unset(CMAKE_PREFIX_PATH)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG FALSE)

# Explicitly set Boost paths to 1.78 installation
set(BOOST_ROOT "C:/libs/boost_1_78_0-bin-msvc-all-32-64/boost_1_78_0")
set(BOOST_INCLUDEDIR "${BOOST_ROOT}")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib64-msvc-14.3")

# Force CMake to use these paths and ignore vcpkg/system
set(Boost_NO_SYSTEM_PATHS ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_COMPILER "-vc143")
set(Boost_ARCHITECTURE "-x64")

# Disable boost auto-linking on Windows
add_definitions(-DBOOST_ALL_NO_LIB)

message(STATUS "Forcing system Boost 1.78 from: ${BOOST_ROOT}")

# For multi-config generators (Visual Studio), find both Release and Debug libraries
# Manually find boost libraries using old-style approach
find_path(Boost_INCLUDE_DIRS
    NAMES boost/version.hpp
    PATHS "${BOOST_ROOT}"
    NO_DEFAULT_PATH)

# Find Release libraries (no suffix)
find_library(Boost_SYSTEM_LIBRARY_RELEASE
    NAMES libboost_system-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_THREAD_LIBRARY_RELEASE
    NAMES libboost_thread-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_FILESYSTEM_LIBRARY_RELEASE
    NAMES libboost_filesystem-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE
    NAMES libboost_program_options-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_REGEX_LIBRARY_RELEASE
    NAMES libboost_regex-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_LOCALE_LIBRARY_RELEASE
    NAMES libboost_locale-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

# Find Debug libraries (-gd suffix)
find_library(Boost_SYSTEM_LIBRARY_DEBUG
    NAMES libboost_system-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_THREAD_LIBRARY_DEBUG
    NAMES libboost_thread-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_FILESYSTEM_LIBRARY_DEBUG
    NAMES libboost_filesystem-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_PROGRAM_OPTIONS_LIBRARY_DEBUG
    NAMES libboost_program_options-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_REGEX_LIBRARY_DEBUG
    NAMES libboost_regex-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_LOCALE_LIBRARY_DEBUG
    NAMES libboost_locale-vc143-mt-gd-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

# Set the main library variables using Release versions for validation
set(Boost_SYSTEM_LIBRARY ${Boost_SYSTEM_LIBRARY_RELEASE})
set(Boost_THREAD_LIBRARY ${Boost_THREAD_LIBRARY_RELEASE})
set(Boost_FILESYSTEM_LIBRARY ${Boost_FILESYSTEM_LIBRARY_RELEASE})
set(Boost_PROGRAM_OPTIONS_LIBRARY ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE})
set(Boost_REGEX_LIBRARY ${Boost_REGEX_LIBRARY_RELEASE})
set(Boost_LOCALE_LIBRARY ${Boost_LOCALE_LIBRARY_RELEASE})

if(Boost_INCLUDE_DIRS AND Boost_SYSTEM_LIBRARY_RELEASE AND Boost_THREAD_LIBRARY_RELEASE AND Boost_FILESYSTEM_LIBRARY_RELEASE AND Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE AND Boost_REGEX_LIBRARY_RELEASE AND Boost_LOCALE_LIBRARY_RELEASE)
    set(Boost_FOUND TRUE)

    # Use optimized library selection for multi-config generators
    set(Boost_LIBRARIES
        optimized ${Boost_SYSTEM_LIBRARY_RELEASE} debug ${Boost_SYSTEM_LIBRARY_DEBUG}
        optimized ${Boost_THREAD_LIBRARY_RELEASE} debug ${Boost_THREAD_LIBRARY_DEBUG}
        optimized ${Boost_FILESYSTEM_LIBRARY_RELEASE} debug ${Boost_FILESYSTEM_LIBRARY_DEBUG}
        optimized ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE} debug ${Boost_PROGRAM_OPTIONS_LIBRARY_DEBUG}
        optimized ${Boost_REGEX_LIBRARY_RELEASE} debug ${Boost_REGEX_LIBRARY_DEBUG}
        optimized ${Boost_LOCALE_LIBRARY_RELEASE} debug ${Boost_LOCALE_LIBRARY_DEBUG})

    message(STATUS "✅ System Boost 1.78 found at: ${Boost_INCLUDE_DIRS}")
    message(STATUS "✅ Boost Release libraries: ${Boost_SYSTEM_LIBRARY_RELEASE};${Boost_THREAD_LIBRARY_RELEASE};${Boost_FILESYSTEM_LIBRARY_RELEASE};${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE};${Boost_REGEX_LIBRARY_RELEASE};${Boost_LOCALE_LIBRARY_RELEASE}")
    message(STATUS "✅ Boost Debug libraries: ${Boost_SYSTEM_LIBRARY_DEBUG};${Boost_THREAD_LIBRARY_DEBUG};${Boost_FILESYSTEM_LIBRARY_DEBUG};${Boost_PROGRAM_OPTIONS_LIBRARY_DEBUG};${Boost_REGEX_LIBRARY_DEBUG};${Boost_LOCALE_LIBRARY_DEBUG}")
else()
    message(STATUS "Include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "System lib (Release): ${Boost_SYSTEM_LIBRARY_RELEASE}")
    message(STATUS "System lib (Debug): ${Boost_SYSTEM_LIBRARY_DEBUG}")
    message(STATUS "Thread lib (Release): ${Boost_THREAD_LIBRARY_RELEASE}")
    message(STATUS "Filesystem lib (Release): ${Boost_FILESYSTEM_LIBRARY_RELEASE}")
    message(STATUS "Program options lib (Release): ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE}")
    message(STATUS "Regex lib (Release): ${Boost_REGEX_LIBRARY_RELEASE}")
    message(STATUS "Locale lib (Release): ${Boost_LOCALE_LIBRARY_RELEASE}")
    message(FATAL_ERROR "❌ System Boost 1.78 not found at specified path")
endif()