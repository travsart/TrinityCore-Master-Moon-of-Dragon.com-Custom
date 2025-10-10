# Technology Stack and Dependencies

## Core Technologies
- **C++ Standard**: C++20 (required)
- **Build System**: CMake 3.24+ minimum
- **Compiler**: 
  - Windows: MSVC 2022 (Visual Studio 17 2022)
  - Linux: GCC or Clang with C++20 support
- **Database**: MySQL 9.4

## Enterprise Dependencies
### Required
- **Intel TBB**: High-performance threading and parallel algorithms
- **Parallel Hashmap**: Optimized concurrent hash maps
- **Boost 1.74+**: Core utility libraries
- **MySQL Client Libraries**: Database connectivity

### Platform-Specific Libraries
- **Windows**: ws2_32, wsock32, mswsock
- **Linux**: pthread, dl

## Build Configuration
The project uses a multi-configuration build system:
- **Debug**: Full debugging symbols, no optimizations
- **Release**: Maximum optimizations (O3/O2), LTO, SIMD
- **RelWithDebInfo**: Optimizations with debug symbols (recommended for development)

## CMake Build Options
- `-DBUILD_PLAYERBOT=1`: Enable PlayerBot module compilation
- `-DBUILD_PLAYERBOT_TESTS=ON`: Enable test compilation
- `-DCMAKE_BUILD_TYPE=Release`: Set build configuration
- `-DCMAKE_TOOLCHAIN_FILE`: For vcpkg on Windows

## Dependency Management
- **Windows**: vcpkg for package management
- **Linux**: System package managers (apt, yum, etc.)
- Dependencies validated via `cmake/modules/FindPlayerbotDependencies.cmake`