using System;
using System.Diagnostics;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

static class Program
{
    static Mutex mutex;

    [STAThread]
    static void Main()
    {
        bool createdNew;
        mutex = new Mutex(true, "CapsLockZhEn", out createdNew);
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

// 低级键盘钩子结构体（用于读取 vkCode 和 flags）
[StructLayout(LayoutKind.Sequential)]
struct KBDLLHOOKSTRUCT
{
    public uint vkCode;
    public uint scanCode;
    public uint flags;
    public uint time;
    public IntPtr dwExtraInfo;
}

// 隐藏窗口：接收 WM_POWERBROADCAST 电源恢复消息
class PowerWindow : NativeWindow
{
    public event Action OnResume;

    public PowerWindow()
    {
        var cp = new CreateParams();
        cp.Caption = "CapsLockZhEn_Power";
        CreateHandle(cp);
    }

    protected override void WndProc(ref Message m)
    {
        const int WM_POWERBROADCAST = 0x0218;
        if (m.Msg == WM_POWERBROADCAST)
        {
            int w = m.WParam.ToInt32();
            // PBT_APMRESUMECRITICAL(6), PBT_APMRESUMESUSPEND(7), PBT_APMRESUMEAUTOMATIC(18)
            if (w == 6 || w == 7 || w == 18)
                if (OnResume != null) OnResume();
        }
        base.WndProc(ref m);
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
    static extern short GetKeyState(int nVirtKey);

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

    [DllImport("imm32.dll")]
    static extern IntPtr ImmGetContext(IntPtr hWnd);

    [DllImport("imm32.dll")]
    static extern bool ImmGetConversionStatus(IntPtr hIMC, out uint lpConversion, out uint lpSentence);

    [DllImport("imm32.dll")]
    static extern bool ImmReleaseContext(IntPtr hWnd, IntPtr hIMC);

    const int WH_KEYBOARD_LL = 13;
    const int WM_KEYDOWN = 0x100;
    const int WM_KEYUP = 0x101;
    const int WM_SYSKEYDOWN = 0x104;
    const int WM_SYSKEYUP = 0x105;
    const uint WM_INPUTLANGCHANGEREQUEST = 0x0050;
    const int VK_CAPITAL = 0x14;
    const int VK_CONTROL = 0x11;
    const int VK_SPACE = 0x20;
    const uint KEYEVENTF_KEYUP = 0x0002;
    const uint LLKHF_INJECTED = 0x0010;
    const uint LANG_CHINESE = 0x0804;
    // IME_CMODE_NATIVE(0x01) 置位表示 IME 处于"中文/汉字输入"模式
    const uint IME_CMODE_NATIVE = 0x0001;

    IntPtr hookId;
    LowLevelKeyboardProc hookProc;
    NotifyIcon tray;
    DateTime capsDown;
    bool capsPressed;
    IntPtr chineseHKL;
    System.Windows.Forms.Timer syncTimer;
    PowerWindow powerWindow;

    public CapsLockCtx()
    {
        DetectChineseLayout();

        hookProc = HookCallback;
        Rehook();

        tray = new NotifyIcon
        {
            Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath),
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

        // 启动时同步 + 定时器每 2 秒检查 + 电源恢复处理
        SyncCapsLock();
        syncTimer = new System.Windows.Forms.Timer { Interval = 2000 };
        syncTimer.Tick += (s, e) => SyncCapsLock();
        syncTimer.Start();

        powerWindow = new PowerWindow();
        powerWindow.OnResume += () => { Rehook(); SyncCapsLock(); };
    }

    // 重新注册钩子（休眠唤醒后系统可能已静默移除）
    void Rehook()
    {
        if (hookId != IntPtr.Zero)
            UnhookWindowsHookEx(hookId);
        using (Process p = Process.GetCurrentProcess())
        using (ProcessModule m = p.MainModule)
            hookId = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc,
                GetModuleHandle(m.ModuleName), 0);
    }

    // 强制 CapsLock 切换状态 OFF
    // 钩子吞掉所有真实 CapsLock 按键，但休眠/唤醒可能将其置为 ON
    // 此时注入一次按键翻转回 OFF（钩子忽略注入事件使其直达系统）
    void SyncCapsLock()
    {
        if ((GetKeyState(VK_CAPITAL) & 1) != 0)
        {
            keybd_event(VK_CAPITAL, 0, 0, UIntPtr.Zero);
            keybd_event(VK_CAPITAL, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        }
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

        var kb = (KBDLLHOOKSTRUCT)Marshal.PtrToStructure(lParam, typeof(KBDLLHOOKSTRUCT));

        if (kb.vkCode == VK_CAPITAL)
        {
            // 放行注入事件（含 SyncCapsLock 自身注入的按键）
            if ((kb.flags & LLKHF_INJECTED) != 0)
                return CallNextHookEx(hookId, nCode, wParam, lParam);

            if (wParam == (IntPtr)WM_KEYDOWN || wParam == (IntPtr)WM_SYSKEYDOWN)
            {
                if (!capsPressed) { capsPressed = true; capsDown = DateTime.Now; }
                return (IntPtr)1;
            }
            if (wParam == (IntPtr)WM_KEYUP || wParam == (IntPtr)WM_SYSKEYUP)
            {
                capsPressed = false;

                // TSF-only 窗口（如 Electron/AutoClaw）：一律只切换中英，
                // 绝不点亮 CapsLock（防止误进大写锁定）。
                if (IsTSFOnlyWindow())
                {
                    ToggleIME();
                    return (IntPtr)1;
                }

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

    // 判断前台线程当前是否真正处于"中文输入"模式。
    // 通过 ImmGetContext/ImmGetConversionStatus 读取前台线程 IME 的当前转换状态：
    // IME_CMODE_NATIVE(0x01) 置位则处于中文输入模式，否则为英文模式。
    bool IsChineseInputMode()
    {
        IntPtr hwnd = GetForegroundWindow();
        if (hwnd == IntPtr.Zero) return false;

        uint pid;
        uint tid = GetWindowThreadProcessId(hwnd, out pid);
        IntPtr cur = GetKeyboardLayout(tid);
        if (((uint)cur & 0xFFFF) != LANG_CHINESE) return false;

        IntPtr imc = ImmGetContext(hwnd);
        if (imc == IntPtr.Zero) return false;

        uint conv, sent;
        bool ok = ImmGetConversionStatus(imc, out conv, out sent);
        ImmReleaseContext(hwnd, imc);

        return ok && (conv & IME_CMODE_NATIVE) != 0;
    }

    // 判断前台窗口是否为"仅有 TSF、无传统 IMM 上下文"的窗口。
    // 这类窗口（典型如 Chromium/Electron，例：AutoClaw）语言 ID 是中文，
    // 但 ImmGetContext 拿不到 / 读不到转换状态（它们走 TSF 文本服务）。
    // 对它们退化为"总是发 Ctrl+Space 翻转"，且绝不让 CapsLock 点亮。
    bool IsTSFOnlyWindow()
    {
        IntPtr hwnd = GetForegroundWindow();
        if (hwnd == IntPtr.Zero) return false;

        uint pid;
        uint tid = GetWindowThreadProcessId(hwnd, out pid);
        IntPtr cur = GetKeyboardLayout(tid);
        if (((uint)cur & 0xFFFF) != LANG_CHINESE) return false;

        IntPtr ime = ImmGetContext(hwnd);
        if (ime == IntPtr.Zero) return true; // 拿不到 input context

        uint conv, sent;
        bool ok = ImmGetConversionStatus(ime, out conv, out sent);
        ImmReleaseContext(hwnd, ime);

        return !ok; // 读不到状态 → 视为 TSF-only
    }

    // 发送一次 Ctrl+Space（用于在 IME 的 中文↔英文 模式间切换）
    void SendCtrlSpace()
    {
        keybd_event(VK_CONTROL, 0, 0, UIntPtr.Zero);
        keybd_event(VK_SPACE, 0, 0, UIntPtr.Zero);
        keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
    }

    void ToggleIME()
    {
        IntPtr hwnd = GetForegroundWindow();
        if (hwnd == IntPtr.Zero) return;

        // 对 Electron/Chromium 这类 TSF-only 窗口：只能靠 Ctrl+Space 翻转，
        // 不可用 WM_INPUTLANGCHANGEREQUEST（消息会被忽略），也不走方向判定。
        if (IsTSFOnlyWindow())
        {
            SendCtrlSpace();
            return;
        }

        uint pid;
        uint tid = GetWindowThreadProcessId(hwnd, out pid);
        IntPtr currentHKL = GetKeyboardLayout(tid);
        uint lang = (uint)currentHKL & 0xFFFF;

        if (lang == LANG_CHINESE && IsChineseInputMode())
        {
            // 当前确实在"中文输入模式"→ 切回英文
            SendCtrlSpace();
        }
        else if (chineseHKL != IntPtr.Zero)
        {
            // 还没有中文布局 → WM_INPUTLANGCHANGEREQUEST 切布局；
            // 或已挂中文布局但 IME 处于英文模式 → 布局已对。
            // 两者都以 Ctrl+Space 兜底。
            PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, IntPtr.Zero, chineseHKL);
            SendCtrlSpace();
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            if (syncTimer != null) syncTimer.Stop();
            if (powerWindow != null) powerWindow.DestroyHandle();
            if (hookId != IntPtr.Zero)
                UnhookWindowsHookEx(hookId);
        }
        base.Dispose(disposing);
    }
}
