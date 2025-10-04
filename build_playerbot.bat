@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" >nul 2>&1
cd /d "C:\TrinityBots\TrinityCore"
msbuild "build\src\server\modules\Playerbot\playerbot.vcxproj" /p:Configuration=Release /verbosity:minimal > build_playerbot_output.txt 2>&1
echo Build completed with exit code %ERRORLEVEL%