@echo off
REM Start Authentication Server
REM Creates auth.db automatically on first run

echo ========================================
echo   Authentication Server
echo ========================================
echo.
echo Starting auth server on port 27016...
echo Database: auth.db
echo.
echo Press Ctrl+C to stop the server
echo.

build\bin\Debug\AuthServer.exe

pause
