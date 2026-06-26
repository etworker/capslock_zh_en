#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0600
#include <windows.h>
#include <shellapi.h>

#define ID_TRAY      100
#define ID_EXIT      101
#define WM_TRAY      (WM_USER+1)
#define LANG_ZHCN    0x0804

static HHOOK g_hook;
static HKL   g_zhHKL;
static int   g_haveZH;
static DWORD g_tDown;

static LRESULT CALLBACK Hook(int code, WPARAM wp, LPARAM lp)
{
    if (code < 0) return CallNextHookEx(g_hook, code, wp, lp);
    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT*)lp;

    if (kb->vkCode == VK_CAPITAL)
    {
        if (wp == WM_KEYDOWN)   { g_tDown = GetTickCount(); return 1; }
        if (wp == WM_KEYUP)
        {
            if (GetTickCount() - g_tDown < 300)
            {
                HWND fg = GetForegroundWindow();
                if (fg)
                {
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
            }
            return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

static LRESULT CALLBACK Wnd(HWND hW, UINT m, WPARAM wp, LPARAM lp)
{
    static NOTIFYICONDATA nd;
    switch (m)
    {
    case WM_CREATE:
        ZeroMemory(&nd, sizeof(nd));
        nd.cbSize = sizeof(nd);
        nd.hWnd = hW; nd.uID = 1;
        nd.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nd.uCallbackMessage = WM_TRAY;
        nd.hIcon = LoadIcon(0, IDI_INFORMATION);
        wcscpy_s(nd.szTip, _countof(nd.szTip), L"CapsLock Zh↔En");
        Shell_NotifyIcon(NIM_ADD, &nd);
        nd.uFlags = NIF_INFO;
        nd.dwInfoFlags = NIIF_INFO;
        wcscpy_s(nd.szInfoTitle, _countof(nd.szInfoTitle), L"CapsLock Zh↔En");
        wcscpy_s(nd.szInfo, _countof(nd.szInfo),
            L"已运行，短按 CapsLock 切换中英文");
        Shell_NotifyIcon(NIM_MODIFY, &nd);
        break;

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
    for (int i=0; i<n; i++)
        if (((ULONG_PTR)buf[i] & 0xFFFF) == LANG_ZHCN)
            { g_zhHKL = buf[i]; g_haveZH = 1; break; }
    free(buf);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int)
{
    HANDLE hm = CreateMutexW(0, FALSE, L"Local\\CapsLockZhEn");
    if (!hm || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hm) CloseHandle(hm);
        return 0;
    }

    DetectLayouts();

    WNDCLASS wc = { .lpfnWndProc=Wnd, .hInstance=hI, .lpszClassName=L"CapsLockZhEn" };
    RegisterClass(&wc);
    HWND hW = CreateWindowEx(0, wc.lpszClassName, 0, 0, 0,0,0,0, 0,0,hI,0);
    if (!hW) return 1;

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, Hook, hI, 0);

    MSG msg;
    while (GetMessage(&msg, 0,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    return 0;
}
