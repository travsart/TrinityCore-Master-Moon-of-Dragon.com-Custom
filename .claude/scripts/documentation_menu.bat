@echo off
REM Quick launcher for PlayerBot Documentation System

echo ================================================================================
echo                    PlayerBot Documentation System
echo ================================================================================
echo.
echo Select an option:
echo.
echo [1] Full Documentation (Analyze + Generate + Insert Comments)
echo [2] Generate Documentation Only (No Comment Insertion)
echo [3] Analyze Code Only
echo [4] Insert Comments (Requires existing documentation.json)
echo [5] Open Existing Documentation
echo [Q] Quit
echo.

set /p choice="Enter your choice: "

set PROJECT_ROOT=C:\TrinityBots\TrinityCore
set SCRIPT_DIR=%PROJECT_ROOT%\.claude\scripts

if "%choice%"=="1" (
    echo.
    echo Starting FULL documentation workflow...
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%\document_project.ps1" -Mode full -AutoFix
    pause
) else if "%choice%"=="2" (
    echo.
    echo Generating documentation without modifying code...
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%\document_project.ps1" -Mode document
    pause
) else if "%choice%"=="3" (
    echo.
    echo Analyzing code structure...
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%\document_project.ps1" -Mode analyze
    pause
) else if "%choice%"=="4" (
    echo.
    echo Inserting comments into code...
    powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%\document_project.ps1" -Mode insert -AutoFix
    pause
) else if "%choice%"=="5" (
    echo.
    echo Opening existing documentation...
    if exist "%PROJECT_ROOT%\.claude\documentation\index.html" (
        start "" "%PROJECT_ROOT%\.claude\documentation\index.html"
    ) else (
        echo No documentation found. Please generate first.
        pause
    )
) else if /i "%choice%"=="Q" (
    echo.
    echo Goodbye!
    exit /b 0
) else (
    echo.
    echo Invalid choice. Please try again.
    pause
    "%~f0"
)
