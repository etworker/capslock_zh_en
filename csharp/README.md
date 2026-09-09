# CapsLock Zh↔En — C# 实现

## 特点

- **7KB 单文件 exe**，零外部依赖
- 通过 `WH_KEYBOARD_LL` 全局钩子监听 CapsLock
- 短按发 `Ctrl+Space` 切换中英文
- Windows 原生 .NET Framework 编译运行
- **单实例保护**：命名互斥锁防止多进程同时运行
- **休眠唤醒恢复**：隐藏窗口监听 `WM_POWERBROADCAST`，唤醒后重新注册钩子并同步状态
- **CapsLock 防锁死**：定时检测并修复 CapsLock 被错误置为 ON 的状态
- **可靠的 IME 模式判断**：通过 `ImmGetConversionStatus` 读取前台线程 IME 的真实中文/英文模式，修复"某些窗口只切大小写"的问题
- **TSF/Electron 适配**：对只有 TSF、无传统 IMM 上下文的窗口（如 Chromium/Electron），退化为"总是发 `Ctrl+Space` 翻转"并禁用长按点亮 CapsLock

## 安装

```
setup.bat
```

## 编译

```cmd
%windir%\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /win32icon:capslock.ico /r:System.Windows.Forms.dll /r:System.Drawing.dll /out:CapsLockZhEn.exe CapsLockZhEn.cs
```

> 图标通过 `/win32icon` 嵌入 exe，托盘图标通过 `Icon.ExtractAssociatedIcon` 从 exe 自身提取。
