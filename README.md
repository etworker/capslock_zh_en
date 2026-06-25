# CapsLock Zh↔En

在 Windows 上实现 **Mac 风格的中英文输入法切换**：短按 `CapsLock` 键，即可在中英文之间切换，无需 `Ctrl+Space` 组合键。

## 解决什么问题

Windows 默认切换输入法需要 `Win+Space` 或 `Ctrl+Space`，而 CapsLock 键在大多数场景下只是用来偶尔输入大写字母（更多人习惯按住 Shift）。

本工具将 **短按 CapsLock** 重映射为切换中英文，**长按 CapsLock** 仍可正常开关大写锁定（300ms 阈值）。既保留了 CapsLock 的原始功能，又提供了随手切换输入法的便利。

## 工作原理

- 通过全局键盘钩子监听 `CapsLock` 按键
- **短按**（< 300ms）→ 切换输入法
  - 当前为中文输入法 → 发送 `Ctrl+Space` 切换到英文模式
  - 当前为英文输入法 → 直接切换到已安装的中文键盘布局
- **长按**（>= 300ms）→ 正常触发大写锁定开关，不做拦截

## 系统要求

- **Windows 11**（主测平台，Windows 10 应该也能用）
- 已安装至少一种中文输入法（如微软拼音）

## 为什么有多个版本

| 方案 | 目录 | 特点 |
|------|------|------|
| **C#** (.NET FW) | `csharp/` | 7KB 单 exe，零依赖，推荐 |
| **C** (Win32) | `c/` | 极小 exe，零依赖，纯 Win32 API |
| **AutoHotkey** | `ahk/` | 需安装 AutoHotkey v2，便于阅读和修改 |

编译后的 exe 统一输出到 `bin/` 目录。

三者功能完全一致，选择哪个取决于你的偏好：

- **C#** — 平衡体积和可维护性，最推荐
- **C** — 追求最小体积和最低资源占用
- **AutoHotkey** — 希望自己改脚本，或者已经在用 AHK

## 快速开始

```cmd
cd csharp
setup.bat
```

或选择其他版本：

```cmd
cd c
setup.bat
```

```cmd
cd ahk
setup.bat    （需先安装 AutoHotkey v2）
```

## 卸载

1. 删除 `启动` 文件夹中的 `CapsLockZhEn.lnk`
2. 或运行对应目录下的 `uninstall.bat`
3. 在系统托盘图标上右键 → 退出

## 编译

```cmd
:: C#
cd csharp
%windir%\Microsoft.NET\Framework\v4.0.30319\csc.exe /target:winexe /r:System.Windows.Forms.dll /r:System.Drawing.dll /out:..\bin\CapsLockZhEn.exe CapsLockZhEn.cs

:: C
cd c
%windir%\System32\cl.exe /nologo /O1 /Fe:..\bin\capslock.exe capslock.c /link /SUBSYSTEM:WINDOWS user32.lib kernel32.lib shell32.lib
```
