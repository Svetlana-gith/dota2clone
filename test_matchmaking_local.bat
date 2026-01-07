@echo off
setlocal enabledelayedexpansion

REM Local Dota-like matchmaking smoke test:
REM - 1x AuthServer (UDP 27016)
REM - 1x MatchmakingCoordinator (UDP 27017)
REM - 1x DedicatedServer registers into pool (game port 27015)
REM - 2x Game clients queue and get MatchReady automatically
REM
REM Usage:
REM   test_matchmaking_local.bat [Debug|Release]
REM
REM Flow:
REM   1) Run this script
REM   2) In BOTH Game windows LOGIN with test1/test1 and test2/test2
REM   3) Press PLAY, wait for "MATCH FOUND"
REM   4) Click ACCEPT in BOTH windows
REM   5) Both clients should transition to Loading and connect to the assigned server

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set ROOT=%~dp0
set BIN=%ROOT%build-vs18\bin\%CONFIG%

if not exist "%BIN%\AuthServer.exe" (
  echo ERROR: "%BIN%\AuthServer.exe" not found. Build first.
  pause
  exit /b 1
)
if not exist "%BIN%\MatchmakingCoordinator.exe" (
  echo ERROR: "%BIN%\MatchmakingCoordinator.exe" not found. Build first.
  pause
  exit /b 1
)
if not exist "%BIN%\DedicatedServer.exe" (
  echo ERROR: "%BIN%\DedicatedServer.exe" not found. Build first.
  pause
  exit /b 1
)
if not exist "%BIN%\Game.exe" (
  echo ERROR: "%BIN%\Game.exe" not found. Build first.
  pause
  exit /b 1
)

echo Starting AuthServer on port 27016...
start "AuthServer" "%BIN%\AuthServer.exe" 27016

REM Give auth server a moment to start
timeout /t 1 >nul

echo Starting MatchmakingCoordinator on port 27017...
start "MatchmakingCoordinator" "%BIN%\MatchmakingCoordinator.exe" 27017 127.0.0.1 27016

REM Give coordinator a moment to bind
timeout /t 1 >nul

echo Starting DedicatedServer (registers to coordinator)...
start "DedicatedServer" "%BIN%\DedicatedServer.exe" 27015 127.0.0.1 27017

REM Give server a moment to register and start heartbeats
timeout /t 1 >nul

echo Starting Game clients (2 windows)...
start "Game A" "%BIN%\Game.exe"
timeout /t 1 >nul
start "Game B" "%BIN%\Game.exe"

echo.
echo DONE. Login with test1/test1 and test2/test2, then press PLAY.
echo AuthServer:   UDP 27016
echo Coordinator:  UDP 27017
echo GameServer:   UDP 27015
echo.
echo Test accounts:
echo   test1 / test1
echo   test2 / test2
echo.
endlocal

