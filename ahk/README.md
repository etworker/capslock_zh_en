# CapsLock Zh↔En — AutoHotkey 实现

## 特点

- 依赖 [AutoHotkey v2](https://www.autohotkey.com/) 运行
- 短按发 `Ctrl+Space` 切换中英文
- `#SingleInstance Force` 天然单实例保护
- **休眠唤醒恢复**：监听 `WM_POWERBROADCAST`，唤醒后同步 CapsLock 状态
- **CapsLock 防锁死**：定时检测并修复 CapsLock 被错误置为 ON 的状态
- **TSF/Electron 适配**：对只有 TSF、无传统 IMM 上下文的窗口（如 Chromium/Electron），退化为"总是发 `Ctrl+Space` 翻转"并禁用长按点亮 CapsLock

## 安装

1. 安装 [AutoHotkey v2](https://www.autohotkey.com/)（无需管理员）
2. 双击 `setup.bat`

## 文件

| 文件 | 作用 |
|------|------|
| `CapsLockZhEn.ahk` | 主脚本 |
| `launch.bat` | 启动器（调用 AHK 运行脚本） |
| `setup.bat` | 安装开机启动 + 运行 |
| `uninstall.bat` | 删除开机启动 |
