# CapsLock Zh↔En

Mac 风格：短按 CapsLock 切换中英文输入法（Windows 11）。

## 实现方案

| 方案 | 目录 | 特点 |
|------|------|------|
| **C#** | `csharp/` | 7KB 单 exe，零依赖，推荐 |
| **AutoHotkey** | `ahk/` | 需安装 AutoHotkey v2 |
| **C** | `c/` | 极小 exe，零依赖，纯 Win32 API |

两者功能完全一致，均通过模拟 `Ctrl+Space` 切换输入法。

## 快速开始

```cmd
cd csharp
setup.bat
```

## 卸载

删除 `启动` 文件夹中的 `CapsLockZhEn.lnk`，或运行对应目录下的 `uninstall.bat`。
