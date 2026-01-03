@echo off
echo ========================================
echo   Starting Game.exe with Console
echo ========================================
echo.

REM Delete old logs
if exist game_debug.log del game_debug.log

REM Run Game.exe (it will show in fullscreen, but logs will appear here)
echo Game is starting in fullscreen...
echo Press ESC in game to exit
echo.
echo === Logs will appear below ===
echo.

build\bin\Release\Game.exe

echo.
echo === Game closed ===
echo.
echo === game_debug.log ===
if exist game_debug.log (
    type game_debug.log
) else (
    echo File not found
)

echo.
pause
