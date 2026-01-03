@echo off
REM Authentication Database Initialization Script
REM This script creates a new auth.db database with the required schema

echo Initializing authentication database...

REM Check if auth.db already exists
if exist auth.db (
    echo WARNING: auth.db already exists!
    set /p confirm="Do you want to overwrite it? (y/n): "
    if /i not "%confirm%"=="y" (
        echo Initialization cancelled.
        exit /b 1
    )
    del auth.db
    echo Existing database deleted.
)

REM Create database using sqlite3
REM Note: This requires sqlite3.exe to be in PATH or in the same directory
sqlite3 auth.db < init_auth_db.sql

if %ERRORLEVEL% EQU 0 (
    echo Database initialized successfully!
    echo Location: %CD%\auth.db
) else (
    echo ERROR: Failed to initialize database
    exit /b 1
)

pause
