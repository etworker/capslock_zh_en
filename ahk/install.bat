@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
title CapsLock ZhEn (AHK) - Install
echo === CapsLock ZhEn (AHK) ===
echo.

:: Check AHK v2
set "AHK_EXE=%LocalAppData%\Programs\AutoHotkey\v2\AutoHotkey64.exe"
if not exist "!AHK_EXE!" (
    echo [NOTICE] AutoHotkey v2 not found - install from https://www.autohotkey.com/
    pause
    exit /b 1
)
echo [OK] AutoHotkey v2 found

set "SCRIPT_DIR=%~dp0"
set "AHK_SCRIPT=!SCRIPT_DIR!CapsLockZhEn.ahk"
set "REG_NAME=CapsLockIME"

echo Adding startup registry entry ...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "%REG_NAME%" /t REG_SZ /d "\"!AHK_EXE!\" \"!AHK_SCRIPT!\"" /f >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    echo [OK] Startup entry added: HKCU\...\Run\%REG_NAME%
) else (
    echo [ERROR] Failed to add registry entry
    pause
    exit /b 1
)

echo Starting CapsLockZhEn.ahk ...
start "CapsLockZhEn" "!AHK_EXE!" "!AHK_SCRIPT!"
echo Done
pause
ENDLOCAL
