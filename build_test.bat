@echo off
cd /d "C:\TrinityBots\TrinityCore"
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" "build\TRINITYCORE.sln" /p:Configuration=Release /verbosity:minimal > build_output.txt 2>&1
echo Build completed with exit code %ERRORLEVEL%