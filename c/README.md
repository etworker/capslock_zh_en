# CapsLock Zh↔En — C 实现

## 特点

- **极小 exe**，零外部依赖
- 通过 `WH_KEYBOARD_LL` 全局钩子监听 CapsLock
- 短按发 `Ctrl+Space` 切换中英文
- 纯 Win32 API，无运行时依赖

## 安装

```
setup.bat
```

## 编译

```cmd
%windir%\System32\cl.exe /nologo /O1 /Fe:capslock.exe capslock.c /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib
```
