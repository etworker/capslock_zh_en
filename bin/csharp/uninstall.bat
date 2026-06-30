@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
title CapsLock ZhEn (C#) - Uninstall
echo === CapsLock ZhEn (C#) - Uninstall ===
echo.

echo Stopping CapsLockZhEn.exe ...
taskkill /f /im CapsLockZhEn.exe >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    echo [OK] Process terminated.
) else (
    echo [INFO] No running process found.
)

set "REG_NAME=CapsLockIME"
echo Removing startup registry entry ...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "%REG_NAME%" /f >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    echo [OK] Startup entry removed.
) else (
    echo [INFO] No startup entry found.
)

echo.
echo Uninstall complete.
echo.
pause
ENDLOCAL
