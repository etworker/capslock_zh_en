@echo off
title CapsLock ZhEn (AHK) - Uninstall
echo === CapsLock ZhEn (AHK) - Uninstall ===
echo.

echo Stopping CapsLockZhEn.ahk ...
taskkill /f /fi "IMAGENAME eq AutoHotkey64.exe" /fi "WINDOWTITLE eq *CapsLockZhEn*" >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    echo [OK] Process terminated.
) else (
    echo [INFO] No running process found.
)

set "LINK_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\CapsLockIME.lnk"
if exist "%LINK_PATH%" (
    del "%LINK_PATH%"
    echo [OK] Startup entry removed.
) else (
    echo [INFO] No startup entry found.
)

echo.
echo Uninstall complete.
echo.
pause
