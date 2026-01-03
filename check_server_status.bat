@echo off
echo ========================================
echo   DedicatedServer Status Check
echo ========================================
echo.

echo [1] Local IP Address:
ipconfig | findstr "IPv4" | findstr "192.168"
echo.

echo [2] Public IP Address:
powershell -Command "Invoke-RestMethod -Uri 'https://api.ipify.org?format=text'"
echo.

echo [3] UDP Port 27015 Status:
netstat -an -p UDP | findstr "27015"
if %errorlevel% equ 0 (
    echo    Status: OPEN
) else (
    echo    Status: CLOSED
)
echo.

echo [4] Server Process:
tasklist | findstr "DedicatedServer.exe"
if %errorlevel% equ 0 (
    echo    Status: RUNNING
) else (
    echo    Status: NOT RUNNING
)
echo.

echo [5] Firewall Rules:
netsh advfirewall firewall show rule name="DedicatedServer UDP" >nul 2>&1
if %errorlevel% equ 0 (
    echo    Status: CONFIGURED
) else (
    echo    Status: NOT CONFIGURED
    echo    Run as Administrator to add rule:
    echo    netsh advfirewall firewall add rule name="DedicatedServer UDP" dir=in action=allow protocol=UDP localport=27015
)
echo.

echo ========================================
echo   Next Steps:
echo ========================================
echo 1. Configure Port Forwarding on router
echo    - External Port: 27015 (UDP)
echo    - Internal IP: [Your Local IP]
echo    - Internal Port: 27015
echo.
echo 2. Start server:
echo    build\bin\Release\DedicatedServer.exe
echo.
echo 3. Connect from outside:
echo    networkClient-^>connect("[Your Public IP]", 27015);
echo.
pause
