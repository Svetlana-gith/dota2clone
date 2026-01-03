@echo off
echo ========================================
echo   Port 27015 Diagnostic Tool
echo ========================================
echo.

echo [1/6] Checking if DedicatedServer is running...
tasklist | findstr "DedicatedServer.exe" >nul
if %errorlevel% equ 0 (
    echo    [OK] Server is RUNNING
) else (
    echo    [FAIL] Server is NOT RUNNING
    echo    Please start: build\bin\Release\DedicatedServer.exe
)
echo.

echo [2/6] Checking UDP port 27015...
netstat -an -p UDP | findstr "27015" >nul
if %errorlevel% equ 0 (
    echo    [OK] Port 27015 is OPEN locally
    netstat -an -p UDP | findstr "27015"
) else (
    echo    [FAIL] Port 27015 is NOT OPEN
)
echo.

echo [3/6] Checking Windows Firewall...
netsh advfirewall firewall show rule name=all | findstr "27015" >nul
if %errorlevel% equ 0 (
    echo    [OK] Firewall rule exists for port 27015
) else (
    echo    [WARN] No firewall rule found
    echo    Run as Administrator:
    echo    netsh advfirewall firewall add rule name="DedicatedServer UDP" dir=in action=allow protocol=UDP localport=27015
)
echo.

echo [4/6] Checking local IP address...
echo    Your local IP:
ipconfig | findstr "IPv4" | findstr "192.168"
echo    Make sure this matches Port Forwarding settings!
echo.

echo [5/6] Checking public IP address...
echo    Your public IP:
powershell -Command "Invoke-RestMethod -Uri 'https://api.ipify.org'"
echo    Friends should connect to this IP
echo.

echo [6/6] Testing local connection...
echo    Attempting to connect to 127.0.0.1:27015...
powershell -ExecutionPolicy Bypass -File test_udp_client.ps1 -ServerIP "127.0.0.1" -Port 27015
echo.

echo ========================================
echo   Summary
echo ========================================
echo.
echo If all checks passed:
echo   1. Server is running
echo   2. Port is open locally
echo   3. Firewall allows connections
echo.
echo Next step: Configure Port Forwarding on router
echo   - Device IP: [Your local IP from step 4]
echo   - External Port: 27015
echo   - Internal Port: 27015
echo   - Protocol: UDP (not TCP!)
echo.
echo To test external connection:
echo   Ask a friend to run: test_external_connection.ps1
echo.
pause
