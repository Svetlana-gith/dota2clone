@echo off
setlocal enabledelayedexpansion

REM Local Dota-like matchmaking smoke test:
REM - 1x MatchmakingCoordinator (UDP 27016)
REM - 1x DedicatedServer registers into pool (game port 27015)
REM - 2x Game clients queue and get MatchReady automatically
REM
REM Usage:
REM   test_matchmaking_local.bat [Debug|Release]
REM
REM Flow:
REM   1) Run this script
REM   2) In BOTH Game windows press PLAY, wait for "MATCH FOUND"
REM   3) Click ACCEPT in BOTH windows
REM   4) Both clients should transition to Loading and connect to the assigned server

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set ROOT=%~dp0
set BIN=%ROOT%build\bin\%CONFIG%

if not exist "%BIN%\MatchmakingCoordinator.exe" (
  echo ERROR: "%BIN%\MatchmakingCoordinator.exe" not found. Build first.
  exit /b 1
)
if not exist "%BIN%\DedicatedServer.exe" (
  echo ERROR: "%BIN%\DedicatedServer.exe" not found. Build first.
  exit /b 1
)
if not exist "%BIN%\Game.exe" (
  echo ERROR: "%BIN%\Game.exe" not found. Build first.
  exit /b 1
)

echo Starting MatchmakingCoordinator...
start "MatchmakingCoordinator" "%BIN%\MatchmakingCoordinator.exe" 27016

REM Give coordinator a moment to bind
timeout /t 1 >nul

echo Starting DedicatedServer (registers to coordinator)...
start "DedicatedServer" "%BIN%\DedicatedServer.exe" 27015 127.0.0.1 27016

REM Give server a moment to register and start heartbeats
timeout /t 1 >nul

echo Starting Game clients (2 windows)...
start "Game A" "%BIN%\Game.exe"
timeout /t 1 >nul
start "Game B" "%BIN%\Game.exe"

echo.
echo DONE. Press PLAY in both clients, then ACCEPT in both.
echo Coordinator: UDP 27016
echo GameServer:  UDP 27015
echo.
endlocal

