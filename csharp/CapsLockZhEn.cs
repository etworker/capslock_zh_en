using System;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Threading;

static class Program
{
    static Mutex mutex;

    [STAThread]
    static void Main()
    {
        mutex = new Mutex(true, "CapsLockZhEn", out bool createdNew);
        if (!createdNew)
        {
            mutex.Close();
            return;
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        using (var ctx = new CapsLockCtx())
            Application.Run();
    }
}

class CapsLockCtx : ApplicationContext
{
    delegate IntPtr LowLevelKeyboardProc(int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    static extern IntPtr SetWindowsHookEx(int idHook, LowLevelKeyboardProc lpfn, IntPtr hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    static extern bool UnhookWindowsHookEx(IntPtr hhk);

    [DllImport("user32.dll")]
    static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("user32.dll")]
    static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    [DllImport("user32.dll")]
    static extern int GetKeyboardLayoutList(int nBuff, [Out] IntPtr[] lpList);

    [DllImport("user32.dll")]
    static extern IntPtr GetKeyboardLayout(uint idThread);

    [DllImport("user32.dll")]
    static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    static extern IntPtr PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    const int WH_KEYBOARD_LL = 13;
    const int WM_KEYDOWN = 0x100;
    const int WM_KEYUP = 0x101;
    const uint WM_INPUTLANGCHANGEREQUEST = 0x0050;
    const int VK_CAPITAL = 0x14;
    const int VK_CONTROL = 0x11;
    const int VK_SPACE = 0x20;
    const uint KEYEVENTF_KEYUP = 0x0002;
    const uint LANG_CHINESE = 0x0804;

    IntPtr hookId;
    LowLevelKeyboardProc hookProc;
    NotifyIcon tray;
    DateTime capsDown;
    bool capsPressed;
    IntPtr chineseHKL;

    public CapsLockCtx()
    {
        DetectChineseLayout();

        hookProc = HookCallback;
        using (Process p = Process.GetCurrentProcess())
        using (ProcessModule m = p.MainModule)
            hookId = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc,
                GetModuleHandle(m.ModuleName), 0);

        tray = new NotifyIcon
        {
            Icon = SystemIcons.Information,
            Text = "CapsLock Zh↔En",
            Visible = true,
        };
        tray.ShowBalloonTip(1500, "CapsLock Zh↔En", "已运行，短按 CapsLock 切换中英文", ToolTipIcon.Info);

        var menu = new ContextMenuStrip();
        menu.Items.Add("退出", null, (s, e) =>
        {
            tray.Visible = false;
            UnhookWindowsHookEx(hookId);
            Application.Exit();
        });
        tray.ContextMenuStrip = menu;
    }

    void DetectChineseLayout()
    {
        int count = GetKeyboardLayoutList(0, null);
        if (count <= 0) return;

        var list = new IntPtr[count];
        GetKeyboardLayoutList(count, list);

        foreach (var hkl in list)
            if (((uint)hkl & 0xFFFF) == LANG_CHINESE)
                { chineseHKL = hkl; break; }
    }

    IntPtr HookCallback(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode < 0)
            return CallNextHookEx(hookId, nCode, wParam, lParam);

        int vk = Marshal.ReadInt32(lParam);

        if (vk == VK_CAPITAL)
        {
            int flags = Marshal.ReadInt32(lParam, 8);
            if ((flags & 0x10) != 0) // LLKHF_INJECTED
                return CallNextHookEx(hookId, nCode, wParam, lParam);

            if (wParam == (IntPtr)WM_KEYDOWN)
            {
                if (!capsPressed) { capsPressed = true; capsDown = DateTime.Now; }
                return (IntPtr)1;
            }
            if (wParam == (IntPtr)WM_KEYUP)
            {
                capsPressed = false;
                if ((DateTime.Now - capsDown).TotalMilliseconds < 300)
                {
                    ToggleIME();
                }
                else
                {
                    ToggleIME();
                    keybd_event(VK_CAPITAL, 0, 0, UIntPtr.Zero);
                    keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
                }
                return (IntPtr)1;
            }
        }

        return CallNextHookEx(hookId, nCode, wParam, lParam);
    }

    void ToggleIME()
    {
        IntPtr hwnd = GetForegroundWindow();
        if (hwnd == IntPtr.Zero) return;

        uint pid;
        uint tid = GetWindowThreadProcessId(hwnd, out pid);
        IntPtr currentHKL = GetKeyboardLayout(tid);
        uint lang = (uint)currentHKL & 0xFFFF;

        if (lang == LANG_CHINESE)
        {
            keybd_event(VK_CONTROL, 0, 0, UIntPtr.Zero);
            keybd_event(VK_SPACE, 0, 0, UIntPtr.Zero);
            keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        }
        else if (chineseHKL != IntPtr.Zero)
        {
            PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, IntPtr.Zero, chineseHKL);
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing && hookId != IntPtr.Zero)
            UnhookWindowsHookEx(hookId);
        base.Dispose(disposing);
    }
}
