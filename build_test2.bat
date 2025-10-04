@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
cd /d "C:\TrinityBots\TrinityCore"
msbuild "build\TRINITYCORE.sln" /p:Configuration=Release /verbosity:minimal > build_output_new.txt 2>&1
echo Build completed with exit code %ERRORLEVEL%