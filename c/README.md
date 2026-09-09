# CapsLock Zh↔En — C 实现

## 特点

- **极小 exe**，零外部依赖
- 通过 `WH_KEYBOARD_LL` 全局钩子监听 CapsLock
- 短按发 `Ctrl+Space` 切换中英文
- 纯 Win32 API，无运行时依赖
- **单实例保护**：命名互斥锁防止多进程同时运行
- **休眠唤醒恢复**：监听电源恢复事件，自动重新注册钩子并同步 CapsLock 状态
- **CapsLock 防锁死**：定时检测并修复 CapsLock 被错误置为 ON 的状态
- **可靠的 IME 模式判断**：通过 `ImmGetConversionStatus` 读取前台线程 IME 的真实中文/英文输入模式，修复"某些窗口只能切大小写"的问题（需链接 `imm32`）
- **TSF/Electron 适配**：对只有 TSF、无传统 IMM 上下文的窗口（如 Chromium/Electron 的 AutoClaw），自动退化为"总是发 `Ctrl+Space` 翻转"，并禁用长按点亮 CapsLock，避免在慢窗口里误进大写锁定

## 编译

> 注：现在多了 `#pragma comment(lib,"imm32.lib")`，链接时需带上 `imm32.lib`。

## 安装

```
setup.bat
```

## 编译

### MSVC (cl.exe)

```cmd
%windir%\System32\cl.exe /nologo /source-charset:utf-8 /O1 /Fe:capslock.exe capslock.c /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib imm32.lib
```

### MinGW (gcc)

```cmd
windres capslock.rc -o capslock_res.o
gcc -O2 -mwindows -o capslock.exe capslock.c capslock_res.o -luser32 -lshell32 -limm32
del capslock_res.o
```

### MSVC (cl.exe)

```cmd
rc capslock.rc
%windir%\System32\cl.exe /nologo /source-charset:utf-8 /O1 /Fe:capslock.exe capslock.c capslock.res /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib imm32.lib
del capslock.res
```

> 图标通过 `capslock.rc` 资源文件嵌入 exe；若未嵌入，运行时也会尝试从 exe 同目录加载 `capslock.ico`，最后回退到系统默认图标。
