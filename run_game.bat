@echo off
if "%~1"=="" (
    echo Drag and drop a GBA ROM file onto this script to play!
    pause
    exit /b
)

echo Starting GBA Emulator with %~1
"build\Release\GBA_Emulator.exe" "%~1"
pause
