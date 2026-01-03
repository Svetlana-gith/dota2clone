@echo off
echo Starting Game.exe with logging...
echo.

REM Delete old logs
if exist game_debug.log del game_debug.log
if exist logs\game.log del logs\game.log

REM Start Game.exe
start "Game" /B build\bin\Release\Game.exe

REM Wait a bit
timeout /t 3 /nobreak >nul

REM Show logs
echo.
echo === game_debug.log ===
if exist game_debug.log (
    type game_debug.log
) else (
    echo File not found
)

echo.
echo === logs\game.log ===
if exist logs\game.log (
    type logs\game.log
) else (
    echo File not found
)

echo.
echo Press any key to close...
pause >nul

REM Kill Game.exe
taskkill /F /IM Game.exe >nul 2>&1
