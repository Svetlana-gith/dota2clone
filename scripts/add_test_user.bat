@echo off
REM Add test user to authentication database
REM Username: testuser
REM Password: password123

echo Adding test user to database...
echo.

build\bin\Debug\AddTestUser.exe

pause
