#Requires AutoHotkey v2.0
#SingleInstance Force

; ================================================================
; CapsLock Zh↔En
; Smart toggle: when Chinese IME is active → Ctrl+Space to switch
; mode; when not Chinese → switch to Chinese IME layout.
; ================================================================

A_TrayMenu.Delete()
A_TrayMenu.Add("Reload", (*) => Reload())
A_TrayMenu.Add("Exit", (*) => ExitApp())
TraySetIcon A_ScriptDir "\capslock.ico"

; ─── Auto-execute section ───
DetectLayouts()
SyncCapsLock()                          ; 启动时强制 CapsLock OFF
SetTimer "SyncCapsLock", 2000           ; 每 2 秒定时同步
OnMessage 0x0218, "OnPowerBroadcast"    ; WM_POWERBROADCAST = 0x0218

; ─── Functions ───

; Detect installed layouts at startup
DetectLayouts() {
    global EnglishHKL := 0, ChineseHKL := 0
    max := 50
    buf := Buffer(max * 4)
    count := DllCall("GetKeyboardLayoutList", "Int", max, "Ptr", buf.Ptr)
    Loop count {
        hkl := NumGet(buf, (A_Index - 1) * 4, "UInt")
        langID := hkl & 0xFFFF
        switch langID {
            case 0x0409: EnglishHKL := EnglishHKL ? EnglishHKL : hkl
            case 0x0804: ChineseHKL := ChineseHKL ? ChineseHKL : hkl
        }
    }
}

; 判断前台窗口是否为"仅有 TSF、无传统 IMM 上下文"的窗口。
; 这类窗口（典型如 Chromium/Electron，例：AutoClaw）语言 ID 是中文（0x0804），
; 但 ImmGetContext 拿不到 / 读不到转换状态（它们走 TSF 文本服务）。
; 对它们退化为"总是 Ctrl+Space 翻转"，且绝不点亮 CapsLock。
IsTSFOnlyWindow() {
    hwnd := WinActive("A")
    if !hwnd
        return false
    threadID := DllCall("GetWindowThreadProcessId", "Ptr", hwnd, "UInt", 0, "Ptr")
    hkl := DllCall("GetKeyboardLayout", "Ptr", threadID, "Ptr")
    if ((hkl & 0xFFFF) != 0x0804)
        return false                       ; 不是中文布局
    imc := DllCall("imm32.dll\ImmGetContext", "Ptr", hwnd, "Ptr")
    if !imc
        return true                        ; 拿不到 input context
    conv := Buffer(4), sent := Buffer(4)
    ok := DllCall("imm32.dll\ImmGetConversionStatus", "Ptr", imc, "Ptr", conv, "Ptr", sent)
    DllCall("imm32.dll\ImmReleaseContext", "Ptr", hwnd, "Ptr", imc)
    return !ok
}

; Force CapsLock toggle OFF.
; CapsLock:: blocks the native key so toggle state should never change,
; but sleep/wake may set it ON externally — this syncs it back.
; Uses Suspend to temporarily remove the hook so keybd_event passes through.
SyncCapsLock() {
    if GetKeyState("CapsLock", "T") {
        Suspend "On"
        DllCall("keybd_event", "UChar", 0x14, "UChar", 0, "UInt", 0, "UPtr", 0)        ; keydown
        DllCall("keybd_event", "UChar", 0x14, "UChar", 0, "UInt", 0x0002, "UPtr", 0)   ; keyup
        Sleep 50
        Suspend "Off"
    }
}

; Power resume handler — sync CapsLock on wake from sleep/hibernate
OnPowerBroadcast(wParam, lParam, msg, hwnd) {
    ; PBT_APMRESUMECRITICAL(6), PBT_APMRESUMESUSPEND(7), PBT_APMRESUMEAUTOMATIC(18)
    if (wParam = 6 || wParam = 7 || wParam = 18)
        SyncCapsLock()
}

; ─── CapsLock handler ───
CapsLock:: {
    ; 在仅 TSF 窗口（如 Electron/Chromium）里，不区分长短按，一律只切换中英，
    ; 并绝不让 CapsLock 落到系统（避免误进大写锁定 / 该窗口自己翻灯）。
    if IsTSFOnlyWindow() {
        ToggleIME()
        return
    }

    KeyWait "CapsLock", "T1"
    if A_TimeSinceThisHotkey >= 300
        return

    ToggleIME()
}

; 在前台窗口执行一次中英切换
ToggleIME() {
    hwnd := WinActive("A")
    if !hwnd
        return

    global ChineseHKL
    threadID := DllCall("GetWindowThreadProcessId", "Ptr", hwnd, "UInt", 0, "Ptr")
    currentHKL := DllCall("GetKeyboardLayout", "Ptr", threadID, "Ptr")
    langID := currentHKL & 0xFFFF

    if (langID = 0x0804) {
        ; Chinese IME → toggle its mode via Ctrl+Space
        Send "{Ctrl down}{Space}{Ctrl up}"
    } else {
        ; English/other → switch to Chinese layout, 然后 Ctrl+Space 兜底
        target := ChineseHKL ? ChineseHKL : 0x08040804
        PostMessage 0x50, 0, target, , "ahk_id " hwnd
        Send "{Ctrl down}{Space}{Ctrl up}"
    }
}
