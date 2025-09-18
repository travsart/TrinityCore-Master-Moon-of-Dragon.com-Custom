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

# Manually find boost libraries using old-style approach
find_path(Boost_INCLUDE_DIRS
    NAMES boost/version.hpp
    PATHS "${BOOST_ROOT}"
    NO_DEFAULT_PATH)

find_library(Boost_SYSTEM_LIBRARY
    NAMES libboost_system-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_THREAD_LIBRARY
    NAMES libboost_thread-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_FILESYSTEM_LIBRARY
    NAMES libboost_filesystem-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_PROGRAM_OPTIONS_LIBRARY
    NAMES libboost_program_options-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_REGEX_LIBRARY
    NAMES libboost_regex-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

find_library(Boost_LOCALE_LIBRARY
    NAMES libboost_locale-vc143-mt-x64-1_78
    PATHS "${BOOST_LIBRARYDIR}"
    NO_DEFAULT_PATH)

if(Boost_INCLUDE_DIRS AND Boost_SYSTEM_LIBRARY AND Boost_THREAD_LIBRARY AND Boost_FILESYSTEM_LIBRARY AND Boost_PROGRAM_OPTIONS_LIBRARY AND Boost_REGEX_LIBRARY AND Boost_LOCALE_LIBRARY)
    set(Boost_FOUND TRUE)
    set(Boost_LIBRARIES ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_LOCALE_LIBRARY})
    message(STATUS "✅ System Boost 1.78 found at: ${Boost_INCLUDE_DIRS}")
    message(STATUS "✅ Boost libraries: ${Boost_LIBRARIES}")
else()
    message(STATUS "Include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "System lib: ${Boost_SYSTEM_LIBRARY}")
    message(STATUS "Thread lib: ${Boost_THREAD_LIBRARY}")
    message(STATUS "Filesystem lib: ${Boost_FILESYSTEM_LIBRARY}")
    message(STATUS "Program options lib: ${Boost_PROGRAM_OPTIONS_LIBRARY}")
    message(STATUS "Regex lib: ${Boost_REGEX_LIBRARY}")
    message(STATUS "Locale lib: ${Boost_LOCALE_LIBRARY}")
    message(FATAL_ERROR "❌ System Boost 1.78 not found at specified path")
endif()