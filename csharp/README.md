# CapsLock Zh↔En — C# 实现

## 特点

- **7KB 单文件 exe**，零外部依赖
- 通过 `WH_KEYBOARD_LL` 全局钩子监听 CapsLock
- 短按发 `Ctrl+Space` 切换中英文
- Windows 原生 .NET Framework 编译运行

## 安装

```
setup.bat
```

## 编译

```cmd
%windir%\Microsoft.NET\Framework\v4.0.30319\csc.exe /target:winexe /r:System.Windows.Forms.dll /r:System.Drawing.dll /out:CapsLockZhEn.exe CapsLockZhEn.cs
```
