@echo off
echo Testing Trade Manager Build...
echo ===============================

cd build

echo Building Trade Manager components...
cmake --build . --config Release --target playerbot 2>&1 | findstr /I "TradeManager error warning"

echo.
echo Build test complete. Check output above for any errors.
pause