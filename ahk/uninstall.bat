@echo off
title CapsLock IME Switcher - Uninstall
echo === CapsLock IME Switcher - Uninstall ===
echo.
set "LINK_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\CapsLockIME.lnk"
if exist "%LINK_PATH%" (
    del "%LINK_PATH%"
    echo [OK] Startup entry removed.
) else (
    echo [INFO] No startup entry found.
)
echo.
echo To stop: right-click tray icon -^> Exit
echo.
pause
