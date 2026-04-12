@echo off
setlocal

:: Resolve VoxCore root by stepping up two directories from tools/shortcuts/
set "ROOT=%~dp0..\.."
for %%i in ("%ROOT%") do set "ROOT=%%~fi"

set "RUNTIME=%ROOT%\out\build\x64-RelWithDebInfo\bin\RelWithDebInfo"
set "MYSQL_DIR=%RUNTIME%\UniServerZ\core\mysql"
set "ARCTIUM=C:\WoW\_retail_\Arctium Game Launcher.exe"
set "CC_DIR=%ROOT%\tools\command-center"

echo ============================================
echo   VoxCore — Starting All Servers
echo ============================================
echo.

:: 1. Start MySQL (UniServerZ)
echo [1/7] Starting MySQL (UniServerZ 9.5.0)...
netstat -ano | findstr ":3306 " | findstr "LISTENING" >nul 2>&1
if %ERRORLEVEL%==0 (
    echo        MySQL already running on port 3306.
    goto mysql_ready
)

start "UniServerZ MySQL" "%MYSQL_DIR%\bin\mysqld_z.exe" ^
    "--defaults-file=%MYSQL_DIR%\my.ini" ^
    "--basedir=%MYSQL_DIR%" ^
    "--datadir=%MYSQL_DIR%\data" ^
    --port=3306 ^
    --console

:: Poll for MySQL readiness (up to 15 seconds, check every 1s)
echo        Waiting for MySQL...
set /a tries=0
:mysql_poll
if %tries% GEQ 15 (
    echo        WARNING: MySQL failed to start on port 3306 after 15 seconds.
    pause
    exit /b 1
)
%SYSTEMROOT%\system32\timeout.exe /t 1 /nobreak >nul
netstat -ano | findstr ":3306 " | findstr "LISTENING" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    set /a tries+=1
    goto mysql_poll
)
echo        UniServerZ MySQL started in ~%tries% seconds.

:mysql_ready
echo.

:: 1.5. Apply Pending SQL
echo [1.5/7] Checking for pending database patches...
call "%~dp0apply_pending_sql.bat"
echo.

:: 2. Start bnetserver
echo [2/7] Starting bnetserver...
start "bnetserver" /D "%RUNTIME%" "%RUNTIME%\bnetserver.exe"
%SYSTEMROOT%\system32\timeout.exe /t 3 /nobreak >nul
echo        bnetserver launched.
echo.

:: 3. Start worldserver
echo [3/7] Starting worldserver...
start "worldserver" /D "%RUNTIME%" "%RUNTIME%\worldserver.exe"
echo        worldserver launched. Waiting for initialization...

:: Poll for worldserver readiness via SOAP port 7878 (up to 90 seconds)
set /a tries=0
:world_poll
if %tries% GEQ 90 (
    echo        WARNING: worldserver SOAP port 7878 not responding after 90 seconds.
    echo        Server may still be loading. Continuing...
    goto world_done
)
%SYSTEMROOT%\system32\timeout.exe /t 2 /nobreak >nul
netstat -ano | findstr ":7878 " | findstr "LISTENING" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    set /a tries+=2
    goto world_poll
)
echo        worldserver ready in ~%tries% seconds.
:world_done
echo.

:: 4. Start Arctium Game Launcher
echo [4/7] Starting Arctium Game Launcher...
if exist "%ARCTIUM%" (
    start "" /D "C:\WoW\_retail_" "%ARCTIUM%"
    echo        Arctium launched.
) else (
    echo        WARNING: Arctium not found at %ARCTIUM%
)
echo.

:: 5. Start Command Center
echo [5/7] Starting Command Center...
netstat -ano | findstr ":5050 " | findstr "LISTENING" >nul 2>&1
if %ERRORLEVEL%==0 (
    echo        Command Center already running on port 5050.
) else (
    start "VoxCore CC" /D "%CC_DIR%" /MIN python app.py
    echo        Command Center started — http://localhost:5050
)
echo.

:: 6. Start Auto-Parse Daemon
echo [6/7] Starting Auto-Parse Session Watcher...
start "VoxCore Auto-Parse v3" /D "%~dp0" auto_parse_watch.bat
echo        Auto-Parse daemon launched.
echo.

:: 7. Start Hook Daemon (Claude Code HTTP hooks)
echo [7/7] Starting VoxCore Hook Daemon...
netstat -ano | findstr ":19484 " | findstr "LISTENING" >nul 2>&1
if %ERRORLEVEL%==0 (
    echo        Hook daemon already running on port 19484.
    goto hook_done
)
start "VoxCore HookDaemon" /MIN python "%ROOT%\.claude\hooks\hook_daemon.py"
%SYSTEMROOT%\system32\timeout.exe /t 2 /nobreak >nul
echo        Hook daemon started — http://127.0.0.1:19484/health
:hook_done
echo.

echo ============================================
echo   All servers started. You can close this.
echo ============================================
%SYSTEMROOT%\system32\timeout.exe /t 5
