@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
title CapsLock ZhEn (C#) - Setup
echo === CapsLock ZhEn (C#) ===
echo.

set "SCRIPT_DIR=%~dp0"
set "EXE_PATH=!SCRIPT_DIR!..\bin\CapsLockZhEn.exe"
set "LINK_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\CapsLockIME.lnk"

if not exist "!EXE_PATH!" (
    echo [ERROR] CapsLockZhEn.exe not found.
    pause
    exit /b 1
)

echo Creating startup shortcut ...
powershell -NoProfile -Command "$WS=New-Object -ComObject WScript.Shell; $SC=$WS.CreateShortcut('!LINK_PATH!'); $SC.TargetPath='!EXE_PATH!'; $SC.WorkingDirectory='!SCRIPT_DIR!'; $SC.Save()"
if not !ERRORLEVEL! EQU 0 (
    echo [ERROR] Failed to create shortcut
    pause
    exit /b 1
)
echo [OK] Startup shortcut: !LINK_PATH!

echo Starting CapsLockZhEn.exe ...
start "CapsLockZhEn" "!EXE_PATH!"
echo Done
pause
ENDLOCAL
