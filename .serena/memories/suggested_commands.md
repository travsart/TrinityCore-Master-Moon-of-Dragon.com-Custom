# Suggested Commands for TrinityCore PlayerBot Development

## Build Commands (Windows)

### Configure CMake
```powershell
# Basic configuration
cmake .. -G "Visual Studio 17 2022" -DBUILD_PLAYERBOT=1

# With vcpkg toolchain
cmake .. -G "Visual Studio 17 2022" `
  -DBUILD_PLAYERBOT=1 `
  -DCMAKE_TOOLCHAIN_FILE="C:/Repositories/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

### Build with MSBuild
```powershell
# Full solution build (Release)
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -maxcpucount:2 `
  TrinityCore.sln

# PlayerBot module only
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -maxcpucount:2 `
  "src\modules\Playerbot\playerbot.vcxproj"

# Worldserver only
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -maxcpucount:2 `
  "src\server\worldserver\worldserver.vcxproj"

# RelWithDebInfo (recommended for development)
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=RelWithDebInfo `
  -p:Platform=x64 `
  -verbosity:minimal `
  TrinityCore.sln
```

### Clean Build
```powershell
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release `
  -p:Platform=x64 `
  -verbosity:minimal `
  -t:Clean `
  TrinityCore.sln
```

## Build Commands (Linux/macOS)

### Configure CMake
```bash
# Basic configuration
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PLAYERBOT=1

# RelWithDebInfo
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_PLAYERBOT=1
```

### Build with Make
```bash
# Parallel build
make -j$(nproc)

# Specific target
make worldserver -j$(nproc)

# Install
cmake --install . --prefix /opt/trinitycore
```

## Test Commands

### Build Tests
```bash
# Configure with tests
cmake .. -DBUILD_PLAYERBOT_TESTS=ON

# Build test target
cmake --build . --target playerbot_tests

# Windows MSBuild
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Debug `
  -p:Platform=x64 `
  playerbot_tests.vcxproj
```

### Run Tests
```bash
# Run all tests
./playerbot_tests

# Run specific test suite
./playerbot_tests --gtest_filter=BehaviorManagerTest.*

# Windows
src\modules\Playerbot\Tests\RUN_BEHAVIORMANAGER_TESTS.bat
```

## Database Commands

### MySQL Connection
```powershell
# Connect to playerbot database
"C:\Program Files\MySQL\MySQL Server 9.4\bin\mysql.exe" `
  -h127.0.0.1 `
  -uplayerbot `
  -pplayerbot `
  playerbot_playerbot

# Check character count
"C:\Program Files\MySQL\MySQL Server 9.4\bin\mysql.exe" `
  -h127.0.0.1 `
  -uplayerbot `
  -pplayerbot `
  playerbot_characters `
  -e "SELECT COUNT(*) as total_characters FROM characters;"
```

## Git Commands

### Standard Git Operations
```bash
git status
git add .
git commit -m "message"
git push
git fetch
git checkout -b branch-name
```

### Common Windows Git
```bash
# Git through Program Files
"C:\Program Files\Git\mingw64\bin\git.exe" status
"C:\Program Files\Git\mingw64\bin\git.exe" commit -m "message"
```

## Utility Commands (Windows)

### File Operations
```powershell
dir                    # List directory
ls                     # PowerShell alias
tree /F               # Show directory tree
copy source dest      # Copy files
mkdir dirname         # Create directory
```

### Process Management
```powershell
tasklist              # List running processes
taskkill /F /IM worldserver.exe  # Kill process
```

### Search
```powershell
findstr /s /i "pattern" *.cpp    # Search in files
where cmake           # Find executable location
```

## Development Workflow

### Typical Development Cycle
```powershell
# 1. Clean build directory (if needed)
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release -p:Platform=x64 -t:Clean TrinityCore.sln

# 2. Build PlayerBot module
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release -p:Platform=x64 -verbosity:minimal -maxcpucount:2 `
  "src\modules\Playerbot\playerbot.vcxproj"

# 3. Build worldserver
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" `
  -p:Configuration=Release -p:Platform=x64 -verbosity:minimal -maxcpucount:2 `
  "src\server\worldserver\worldserver.vcxproj"

# 4. Run tests (if available)
cmake --build . --target playerbot_tests

# 5. Git commit
git add .
git commit -m "[PlayerBot] Description of changes"
git push
```