@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
title CapsLock ZhEn (C) - Install
echo === CapsLock ZhEn (C) ===
echo.

set "SCRIPT_DIR=%~dp0"
set "EXE_PATH=!SCRIPT_DIR!CapsLockZhEn.exe"
set "REG_NAME=CapsLockIME"

if not exist "!EXE_PATH!" (
    echo [ERROR] CapsLockZhEn.exe not found.
    pause
    exit /b 1
)

echo Adding startup registry entry ...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "%REG_NAME%" /t REG_SZ /d "!EXE_PATH!" /f >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    echo [OK] Startup entry added: HKCU\...\Run\%REG_NAME%
) else (
    echo [ERROR] Failed to add registry entry
    pause
    exit /b 1
)

echo Starting CapsLockZhEn.exe ...
start "CapsLockZhEn" "!EXE_PATH!"
echo Done
pause
ENDLOCAL
