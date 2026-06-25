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
TraySetIcon "imageres.dll", 132

; ─── Detect installed layouts at startup ───
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

DetectLayouts()

; ─── CapsLock handler ───
CapsLock:: {
    KeyWait "CapsLock", "T1"
    if A_TimeSinceThisHotkey >= 300
        return

    ; Get current layout for active window
    hwnd := WinActive("A")
    if !hwnd
        return

    threadID := DllCall("GetWindowThreadProcessId", "Ptr", hwnd, "UInt", 0)
    currentHKL := DllCall("GetKeyboardLayout", "Ptr", threadID, "Ptr")
    langID := currentHKL & 0xFFFF

    if (langID = 0x0804) {
        ; Chinese IME → toggle its mode via Ctrl+Space
        Send "{Ctrl down}{Space}{Ctrl up}"
    } else {
        ; English/other → switch to Chinese layout
        global ChineseHKL
        target := ChineseHKL ? ChineseHKL : 0x08040804
        PostMessage 0x50, 0, target, , "ahk_id " hwnd
    }
}
