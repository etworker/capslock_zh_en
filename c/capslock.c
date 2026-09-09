#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0600
#include <windows.h>
#include <shellapi.h>
#include <imm.h>
#include <stdlib.h>

#pragma comment(lib, "imm32.lib")

#define ID_TRAY        100
#define ID_EXIT        101
#define WM_TRAY        (WM_USER+1)
#define LANG_ZHCN      0x0804
#define MUTEX_NAME      L"CapsLockZhEn_SingleInstance"
#define TIMER_SYNC     2
#define SYNC_INTERVAL  2000  /* ms — 定时同步 CapsLock 状态 */

/* IME 转换状态标志：IME_CMODE_NATIVE(0x01) 置位表示处于"中文/汉字输入"模式 */
#define IME_CMODE_NATIVE 0x0001

static HHOOK      g_hook;
static HKL        g_zhHKL;
static int        g_haveZH;
static DWORD      g_tDown;
static BOOL       g_capsDown;
static HINSTANCE  g_hInst;

/*
 * 判断前台线程当前是否真正处于"中文输入"模式。
 *
 * 之前版本只通过语言 ID（0x0804）判断，但一个中文布局（HKL）内部
 * 还区分"英文模式(alphanumeric)"和"中文/汉字模式(native)"两种状态。
 * 当窗口停在中文字体的英文模式时，语言 ID 虽是 0x0804，实际却在打英文——
 * 直接发 Ctrl+Space 无法保证进入中文，导致表现成"只能切大小写"。
 *
 * 这里用 ImmGetContext + ImmGetConversionStatus 读取前台线程 IME 的
 * 当前转换状态：IME_CMODE_NATIVE(0x01) 置位→中文模式，清空→英文模式。
 */
static int IsChineseInputMode(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;

    DWORD pid, tid = GetWindowThreadProcessId(fg, &pid);
    HKL cur = GetKeyboardLayout(tid);

    /* 只有当前确实是中文语言时才有意义 */
    if (((ULONG_PTR)cur & 0xFFFF) != LANG_ZHCN)
        return 0;

    HIMC imc = ImmGetContext(fg);
    if (!imc) return 0;

    DWORD conv = 0, sent = 0;
    BOOL ok = ImmGetConversionStatus(imc, &conv, &sent);
    ImmReleaseContext(fg, imc);

    if (!ok) return 0;
    return (conv & IME_CMODE_NATIVE) ? TRUE : FALSE;
}

/* 发送一次 Ctrl+Space（用于在 IME 的 中文↔英文 模式间切换） */
static void SendCtrlSpace(void)
{
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event(VK_SPACE,   0, 0, 0);
    keybd_event(VK_SPACE,   0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

/*
 * 判断前台窗口是否为"仅有 TSF、无传统 IMM 上下文"的窗口。
 *
 * 这类窗口（典型如 Chromium / Electron，例如 AutoClaw）：
 *  - 语言 ID 是中文（0x0804）；
 *  - 但 ImmGetContext 返回 NULL / ImmGetConversionStatus 读不到状态，
 *    因为它们走 TSF(Text Services Framework)，不暴露旧式 IMM 输入上下文。
 *
 * 对这类窗口来说，用语言 ID + IMM 判方向都不可靠，布局切换消息
 * (WM_INPUTLANGCHANGEREQUEST) 也常被丢弃。因此对它们统一退化为
 * "总是发 Ctrl+Space 翻转"，且不要让 CapsLock 点亮（否则表现为误进
 * 大写锁定）。
 */
static int IsTSFOnlyWindow(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;

    DWORD pid, tid = GetWindowThreadProcessId(fg, &pid);
    HKL cur = GetKeyboardLayout(tid);
    if (((ULONG_PTR)cur & 0xFFFF) != LANG_ZHCN)
        return 0;   /* 不是中文，与 TSF 场景无关 */

    /* 中文布局但拿不到 IMM 上下文 → 极可能是 Electron/Chromium 的 TSF 界面 */
    HIMC imc = ImmGetContext(fg);
    if (!imc) return 1;   /* 拿不到 input context */

    DWORD conv = 0, sent = 0;
    BOOL ok = ImmGetConversionStatus(imc, &conv, &sent);
    ImmReleaseContext(fg, imc);

    /* 能正常读到状态 → 用普通路径即可；读不到 → 视为 TSF-only */
    return ok ? 0 : 1;
}

static void ToggleIME(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return;

    /* 对 Electron/Chromium 这类 TSF-only 窗口：只能靠 Ctrl+Space 翻转，
       不可用 WM_INPUTLANGCHANGEREQUEST（消息会被忽略），也不走方向判定。 */
    if (IsTSFOnlyWindow())
    {
        SendCtrlSpace();
        return;
    }

    if (IsChineseInputMode())
    {
        /* 当前线程确实在"中文输入模式"→ 切回英文模式 */
        SendCtrlSpace();
    }
    else if (g_haveZH)
    {
        /*
         * 两种情况都走到这里：
         *  a) 线程还没有中文布局 → 用 WM_INPUTLANGCHANGEREQUEST 切换布局；
         *  b) 线程已挂中文布局但 IME 处在英文模式 → 布局已对，只需模式翻转。
         * 两者都以 Ctrl+Space 兜底：布局切换后立即把它落到中文输入模式，
         * 也兼顾那些会忽略 WM_INPUTLANGCHANGEREQUEST 的窗口。
         */
        PostMessage(fg, 0x0050, 0, (LPARAM)g_zhHKL);
        SendCtrlSpace();
    }
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

            /* TSF-only 窗口（如 Electron/AutoClaw）：一律只切换中英，
               绝不点亮 CapsLock（防止误进大写锁定）。 */
            if (IsTSFOnlyWindow())
            {
                ToggleIME();
                return 1;
            }

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

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    /* 单实例检查：防止多进程同时运行 */
    HANDLE hm = CreateMutexW(0, FALSE, L"Local\\CapsLockZhEn");
    if (!hm || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hm) CloseHandle(hm);
        return 0;
    }

    DetectLayouts();

    g_hInst = hInst;
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = Wnd;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CapsLockZhEn";
    RegisterClass(&wc);
    HWND hW = CreateWindowEx(0, wc.lpszClassName, 0, 0, 0,0,0,0, 0,0,hInst,0);
    if (!hW) return 1;

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, Hook, hInst, 0);

    MSG msg;
    while (GetMessage(&msg, 0,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    if (hm) CloseHandle(hm);
    return 0;
}
