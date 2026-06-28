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
set "LINK_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\CapsLockIME.lnk"

echo Creating startup shortcut ...
powershell -NoProfile -Command "$WS=New-Object -ComObject WScript.Shell; $SC=$WS.CreateShortcut('!LINK_PATH!'); $SC.TargetPath='!AHK_EXE!'; $SC.Arguments='\"!AHK_SCRIPT!\"'; $SC.WorkingDirectory='!SCRIPT_DIR!'; $SC.Save()"
if not !ERRORLEVEL! EQU 0 (
    echo [ERROR] Failed to create shortcut
    pause
    exit /b 1
)
echo [OK] Startup shortcut: !LINK_PATH!

echo Starting CapsLockZhEn.ahk ...
start "CapsLockZhEn" "!AHK_EXE!" "!AHK_SCRIPT!"
echo Done
pause
ENDLOCAL
