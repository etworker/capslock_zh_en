#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0600
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>

#define ID_TRAY        100
#define ID_EXIT        101
#define WM_TRAY        (WM_USER+1)
#define LANG_ZHCN      0x0804
#define MUTEX_NAME     L"CapsLockZhEn_SingleInstance"
#define TIMER_SYNC     2
#define SYNC_INTERVAL  2000  /* ms — 定时同步 CapsLock 状态 */

static HHOOK      g_hook;
static HKL        g_zhHKL;
static int        g_haveZH;
static DWORD      g_tDown;
static BOOL       g_capsDown;
static HINSTANCE  g_hInst;

static void ToggleIME(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    DWORD pid, tid = GetWindowThreadProcessId(fg, &pid);
    HKL cur = GetKeyboardLayout(tid);
    if (((ULONG_PTR)cur & 0xFFFF) == LANG_ZHCN)
    {
        keybd_event(VK_CONTROL,0,0,0);
        keybd_event(VK_SPACE,  0,0,0);
        keybd_event(VK_SPACE,  0,KEYEVENTF_KEYUP,0);
        keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0);
    }
    else if (g_haveZH)
        PostMessage(fg, 0x0050, 0, (LPARAM)g_zhHKL);
}

/*
 * 强制将 CapsLock 切换状态置为 OFF。
 *
 * 原理：本程序吞掉所有真实的 CapsLock 按键，所以正常情况下
 * 系统的 CapsLock 切换状态永远不应为 ON。但休眠/唤醒期间
 * 键盘驱动可能在钩子未激活时注入了 CapsLock 事件，导致
 * 切换状态被错误地置为 ON——此时用户按 CapsLock 也无法关闭
 * 因为钩子继续吞掉所有事件。
 *
 * 修复：检测到 ON 时注入一次 CapsLock 按下+抬起；钩子会
 * 忽略 LLKHF_INJECTED 标记的事件使其直达系统，从而翻转回 OFF。
 */
static void SyncCapsLock(void)
{
    if (GetKeyState(VK_CAPITAL) & 0x0001)
    {
        keybd_event(VK_CAPITAL, 0, 0,             0);
        keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP,0);
    }
}

static LRESULT CALLBACK Hook(int code, WPARAM wp, LPARAM lp)
{
    if (code < 0) return CallNextHookEx(g_hook, code, wp, lp);
    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT*)lp;

    if (kb->vkCode == VK_CAPITAL)
    {
        /* 放行注入事件（含 SyncCapsLock 自身注入的按键） */
        if (kb->flags & LLKHF_INJECTED)
            return CallNextHookEx(g_hook, code, wp, lp);

        if (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)
        {
            if (!g_capsDown) { g_capsDown = 1; g_tDown = GetTickCount(); }
            return 1;
        }
        if (wp == WM_KEYUP || wp == WM_SYSKEYUP)
        {
            g_capsDown = 0;
            if (GetTickCount() - g_tDown < 300)
                ToggleIME();
            else
            {
                ToggleIME();
                keybd_event(VK_CAPITAL, 0, 0, 0);
                keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0);
            }
            return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

/* 重新注册钩子——休眠唤醒后系统可能已静默移除它 (LowLevelHooksTimeout) */
static void Rehook(void)
{
    if (g_hook) UnhookWindowsHookEx(g_hook);
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, Hook, g_hInst, 0);
}

/* 加载应用图标：优先嵌入资源 → 其次 .ico 文件 → 最后系统图标 */
static HICON LoadAppIcon(void)
{
    /* 1. 尝试嵌入资源 (capslock.rc 中 ID=1) */
    HICON hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
    if (hIcon) return hIcon;

    /* 2. 尝试从 exe 同目录加载 capslock.ico */
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH))
    {
        wchar_t *p = wcsrchr(path, L'\\');
        if (p)
        {
            wcscpy_s(p + 1, MAX_PATH - (p + 1 - path), L"capslock.ico");
            hIcon = (HICON)LoadImageW(NULL, path, IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON),
                        LR_LOADFROMFILE);
            if (hIcon) return hIcon;
        }
    }

    /* 3. 回退到系统图标 */
    return LoadIcon(0, IDI_APPLICATION);
}

static LRESULT CALLBACK Wnd(HWND hW, UINT m, WPARAM wp, LPARAM lp)
{
    static NOTIFYICONDATA nd;

    switch (m)
    {
    case WM_CREATE:
        /* 托盘图标 */
        ZeroMemory(&nd, sizeof(nd));
        nd.cbSize = sizeof(nd);
        nd.hWnd = hW; nd.uID = 1;
        nd.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nd.uCallbackMessage = WM_TRAY;
        nd.hIcon = LoadAppIcon();
        wcscpy_s(nd.szTip, _countof(nd.szTip), L"CapsLock Zh↔En");
        Shell_NotifyIcon(NIM_ADD, &nd);
        nd.uFlags = NIF_INFO;
        nd.dwInfoFlags = NIIF_INFO;
        wcscpy_s(nd.szInfoTitle, _countof(nd.szInfoTitle), L"CapsLock Zh↔En");
        wcscpy_s(nd.szInfo, _countof(nd.szInfo),
            L"已运行，短按 CapsLock 切换中英文");
        Shell_NotifyIcon(NIM_MODIFY, &nd);

        /* 启动时强制 CapsLock OFF，并开启定时同步 */
        SyncCapsLock();
        SetTimer(hW, TIMER_SYNC, SYNC_INTERVAL, NULL);
        break;

    case WM_TIMER:
        if (wp == TIMER_SYNC)
            SyncCapsLock();
        break;

    case WM_POWERBROADCAST:
        /* 系统从睡眠/休眠恢复时重新注册钩子并同步 CapsLock */
        if (wp == PBT_APMRESUMEAUTOMATIC ||
            wp == PBT_APMRESUMESUSPEND  ||
            wp == PBT_APMRESUMECRITICAL)
        {
            Rehook();
            SyncCapsLock();
        }
        return TRUE;

    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP)
        {
            HMENU mu = CreatePopupMenu();
            AppendMenu(mu, MF_STRING, ID_EXIT, L"退出");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hW);
            if (TrackPopupMenu(mu, TPM_RETURNCMD,pt.x,pt.y,0,hW,0) == ID_EXIT)
                PostQuitMessage(0);
            DestroyMenu(mu);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hW, TIMER_SYNC);
        Shell_NotifyIcon(NIM_DELETE, &nd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hW, m, wp, lp);
}

static void DetectLayouts()
{
    int n = GetKeyboardLayoutList(0, 0);
    if (n <= 0) return;
    HKL *buf = (HKL*)malloc(n * sizeof(HKL));
    if (!buf) return;
    GetKeyboardLayoutList(n, buf);
    int i;
    for (i=0; i<n; i++)
        if (((ULONG_PTR)buf[i] & 0xFFFF) == LANG_ZHCN)
            { g_zhHKL = buf[i]; g_haveZH = 1; break; }
    free(buf);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int)
{
    /* 单实例检查：防止多进程同时运行 */
    HANDLE hm = CreateMutexW(0, FALSE, L"Local\\CapsLockZhEn");
    if (!hm || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hm) CloseHandle(hm);
        return 0;
    }

    DetectLayouts();

    g_hInst = hI;
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = Wnd;
    wc.hInstance = hI;
    wc.lpszClassName = L"CapsLockZhEn";
    RegisterClass(&wc);
    HWND hW = CreateWindowEx(0, wc.lpszClassName, 0, 0, 0,0,0,0, 0,0,hI,0);
    if (!hW) return 1;

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, Hook, hI, 0);

    MSG msg;
    while (GetMessage(&msg, 0,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    if (hm) CloseHandle(hm);
    return 0;
}
