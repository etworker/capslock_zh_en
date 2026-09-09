# CapsLock 中英文切换在 Electron/Chromium 窗口失效 —— 技术调研与决策记录

> 结论先行：**在 Electron/Chromium 的网页输入框里，用纯软件方式统一切换中英文，技术上做不到且不值得做。**
> 本工具保持现状；若将来需要"全窗口（含 Electron）都用 CapsLock 切中英"，唯一稳定方案是**驱动层注册表 Scancode Map**
> （详见文末「方案 A」）。本次未改动系统配置。

---

## 1. 背景与现象

工具（CapsLock Zh↔En，有 C / C# / AHK 三个实现）用全局低级键盘钩子 `WH_KEYBOARD_LL`
把 CapsLock 重映射为"中英输入法切换"：

- 短按 CapsLock（<300ms）→ 切换 中文 ↔ 英文（发 `Ctrl+Space`，或 `WM_INPUTLANGCHANGEREQUEST`）；
- 长按（≥300ms）→ 正常触发大写锁定。

**问题现象**：在**绝大多数原生窗口**（记事本、资源管理器、常规 IDE 等）都能正常"短按切中英"；
但在 **AutoClaw** 的输入框里按 CapsLock，行为异常——表现为"灯亮/灭 + 大写英文/中文 随动"，
且**不受工具开关影响**（工具退出后现象不变）。

AutoClaw 是一个 **Electron（Chromium）** 应用（`C:\Program Files\AutoClaw`，含 `app.asar`、
`resources.pak` 等），由「北京智谱华章科技股份有限公司」（AutoGLM / GLM 系）发布。

---

## 2. 诊断过程（已实测）

### 2.1 探测 AutoClaw 窗口的 IME 状态

用 user32/imm32 API 枚举 AutoClaw 进程（PID 2396 主进程 + 若干渲染进程）的窗口并读取输入状态：

```
AutoClaw 主窗口 (classe=Chrome_WidgetWin_1)
  HKL    = 0x8040804        （语言 ID 0x0804 = 简体中文）
  IME    = no-imm           （ImmGetContext 返回 NULL —— 没有传统 IMM 输入上下文）
```

结论：AutoClaw 是 Chromium/Electron，其输入由 **TSF（Text Services Framework）** 接管，
**不暴露旧式 IMM（IMM32）输入上下文**。因此基于 `HKL + ImmGet(ConversionStatus / OpenStatus)`
的判定在 AutoClaw 里不可用。

### 2.2 工具日志（临时在 KEYUP 里记录每次 CapsLock）

给 C 版本临时加了调试日志（记录 `IsTSFOnlyWindow()` 判定、按键时长），实测 AutoClaw 里的按键：

```
mode keyup pid=6984 hkl=0x8040804 lang=0x804 imc=null isTSF=1 delta=62  # 多次如此
```

即：工具能正确识别 AutoClaw 为"仅 TSF 窗口"（`isTSF=1`），短按时长合理（62ms）。
审计到这一支之后工具**只发 `Ctrl+Space`、绝不注入 CapsLock**。
但即便如此，AutoClaw 内的"灯亮/灭 + 大写/中文"现象依然存在——**与工具无关**。

### 2.3 决定性的对照实验

**退出工具**后，在 AutoClaw 输入框按 CapsLock：

- 灯亮/灭、项目也随动（灯亮→大写英文，灯灭→中文）；
- 现象与工具运行时**完全一致**。

**结论**：AutoClaw 的 web 输入框**自己就接管了 CapsLock**（用作"中英/大小写组合切换"），
与工具开关无关。**工具在 AutoClaw 里既插不上手、也不能改变它的行为。**

---

## 3. 根因：为什么纯软件方案在 Chromium 里注定失效

| 层级 | 说明 |
|------|------|
| **真实的捕获层** | Chromium/Electron 通过**自己的渲染管线 + TSF/ITfTextStore** 驱动 IME，不经过经典 IMM32 消息通道。 |
| **可靠的证据** | 微软 [WebView2 issue #5637](https://github.com/MicrosoftEdge/WebView2Feedback/issues/5637)：Chromium 会**忽略一切"程序化"的 IME 模式切换**（`SendInput Ctrl+Space`、`ImmSetConversionStatus`、`WM_IME_CONTROL`/`IMC_SETCONVERSIONMODE`、`WM_INPUTLANGCHANGEREQUEST`），**只认真实硬件键**。 |
| **旁证** | Qiita（Windows IME 检测）指出 IMM32（`ImmGetContext`/`ImmGetOpenStatus`）**在 Electron 里不可靠**（且 Windows Terminal/PowerShell/console 长期返回 false）。 |
| **本项目实测** | AutoClaw 窗口 `ImmGetContext = NULL`（只走 TSF，无 IMM 上下文）。 |

因此：**当前工具用 PostMessage/`Ctrl+Space` 等注入的都是"虚拟事件"，Chromium 内部把它丢弃，
不改变网页输入框内的中/英转换模式**。这不是代码 bug，而是 Chromium 输入管线的最底层限制。

---

## 4. 全窗口通用方案（若将来需要，可复用）

### 核心原则

要让"所有窗口（含 Electron）按一个键切中英"，唯一稳定路径是**载体层级**实现：
不通过用户态挂钩/注入，而是让 CapsLock 在"任何程序之下"就变成别的键（或消失）。

### 方案 A（推荐，真正通用、零常驻开销）—— 注册表 `Scancode Map`

`Scancode Map` 由**键盘类驱动在开机时读取一次**，作用于原始扫描码流，
**位于所有应用、输入钩子、反作弊、登录界面之下**。它把 CapsLock 变成"目的"键，
对包括 Electron/Chromium 在内的**每一个应用**都是一致生效的真实硬件键。

- ekinertac《I Taught Windows To Speak Mac, One Scancode At A Time》：
  https://www.ekinertac.com/blog/teaching-windows-to-speak-mac/
- Qiita（注册表 vs AutoHotkey，注册表更稳）：
  https://qiita.com/tubo28/items/8909a8ce4ca2524b8cf7
- CSDN《对比 Windows CapsLock 切换中英的三种方式》—— 明确：**注册表映射最稳定**，
  PowerToys/AutoHotkey 会出现"开机久了 CapsLock 按下失效（按键状态锁死，只能重启）"的 bug：
  https://blog.csdn.net/qq_25846269/article/details/143815215
- 微软官方（禁用 CapsLock 的精确 reg 值，映射 0x3A→0x00）：
  https://learn.microsoft.com/en-us/answers/questions/2525250/is-it-possible-to-disable-the-caps-lock-key-on-a-l

#### A.1 最简单形式：**禁用它**（`空`）
```reg
Windows Registry Editor Version 5.00
[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layout]
"Scancode Map"=hex:00,00,00,00,00,00,00,00,02,00,00,00,00,00,3a,00,00,00,00,00
```
作用：CapsLock 扫描码 0x3A → 0x00（禁用）。从此**没有任何应用能收到 CapsLock**，
AutoClaw 的"灯/大写/中英随动"问题在根源上消失。

#### A.2 更好形式：**把 CapsLock 映射成"你能设置在 IME 里的单个"键**

注册表只能做"**键→键的单键映射**"，做不了"键→Ctrl+Space 组合键"。做法：

1. 把中文输入法（如微软拼音）的「中/英」切换键设为一个**单独键**：
   - 微软拼音：`HKCU\Software\Microsoft\InputMethod\Settings\CHS` 的 `English Switch Key`
     > 0 = Shift，1 = Ctrl，2 = 无（禁用）。详见 https://www.cnblogs.com/BluePointLilac/p/11688166.html
2. 用 `Scancode Map` 把 **CapsLock（0x3A）→ 那个单键**。映射记录格式为 4 字节小端：
   `<目的键码>, <源键码>`。例如映射到右 Shift（SC 0x36）：
   ```
   00,00,00,00,00,00,00,00,   ; 头
   02,00,00,00,               ; 2 项：1 条映射 + 终止
   36,00,3a,00,               ; CapsLock(3a) -> RightShift(36)
   00,00,00,00                ; 终止
   ```
3. 重启生效（驱动开机读取一次）。

效果：CapsLock 变成 Shift，微软拼音收到的是一个**真实物理键**，
在所有窗口（含 Electron）都能用它切中英。代价：需一次落地；CapsLock 不再是锁大写键。

#### A.3 操作提示
- 必须**管理员**导入并 **重启**；
- 适用**机器上所有键盘**，**永久**生效，**没有按窗口**区分；
- 工具若继续保留，CapsLock 不再经过 `WH_KEYBOARD_LL`（已变成别的键），与工具互不冲突；
- 禁用 ency：每次上需要用同样的注册表把映射去掉并重启。

#### A.4 关键开发：Windows 系统配置，非本工具能力
`Scancode Map` 属于**系统配置**，与 CapsLock Zh↔En 工具的代码无关。
如果将来要做，建议提供工具内的"安装/卸载 Scancode Map + 提示重启"，让用户有可视可控。

### 方案 B（项目现状，已采用）—— 工具保持现状，不针对 Electron 做适配

- 工具保留已有的"仅 **TSF 窗口不翻灯、只切中英**"处理（见 `c/` 实现：`IsTSFOnlyWindow()`）；
- 普通原生窗口一切正常；AutoClaw 这类 Chromium 输入框**让它用自己的行为**；
- **零额外开销、零稳定性风险**，符合"不为小众软件的不兼容增加成本"的原则。

> 结论对我们项目的落地：**保持现状（方案 B）。** 若未来有全窗口通用需求，再评估方案 A。

---

## 5. 对代码现状的记录（本调研进行中的变更，至今）

仅在 **C 版**做了以下改动（已重编，`bin\c\CapsLockZhEn.exe`）：

1. `c\capslock.c`：
   - 引入 `ImmGetContext`/`ImmGetConversionStatus`，新增 `IsChineseInputMode()` 及 `IsTSFOnlyWindow()`；
   - `ToggleIME()`：对"仅 TSF/无 IMM 上下文"的窗口（如 Chromium/Electron）退化为**总是发 Ctrl+Space**，不做方向误判；
   - 在新窗口的 KEYUP 分支强制 **不点亮 CapsLock**（防"在慢窗口里被误进成大写锁");
   - 修正了一个**预先存在的** MSVC 编译问题：`WinMain` 参数未命名导致 C2055（MSVC 2019 下无法编译）；
   - 高级编译命令需链接 `imm32`（`imm32.lib` / `-limm32`）。
2. `c\README.md`：补充了 TSF/Electron 适配说明。

> 注意：**这些改动并不能修复 AutoLock 内的体验**——因为问题在 Chromium 输入管线的底层，
> 工具无法越过；这些改动只能减少"普通或 TSF 窗口里的误操作"。AutoClaw 的行为只能由方案 A 或 AutoClaw 自身设置解决。

---

## 6. 参考来源汇总

1. WebView2 / Chromium 忽略程序化 IME 切换：https://github.com/MicrosoftEdge/WebView2Feedback/issues/5637
2. Qiita — 在 Electron 里 IMM32 不可靠：https://qiita.com/obott/items/21e71dab752ae65e713b
3. 微软问答 — 禁用 CapsLock：https://learn.microsoft.com/en-us/answers/questions/2525250/is-it-possible-to-disable-the-caps-lock-key-on-a
4. ekinertac — Scancode Map 驱动层解释：https://www.ekinertac.com/blog/teaching-windows-to-speak-mac/
5. Qiita — 注册表 Scancode Map 更稳：https://qiita.com/tubo28/items/8909a8ce4ca2524b8cf7
6. CSDN — 三种方式对比（注册表最稳定）：https://blog.csdn.net/qq_25846269/article/details/143815215
7. CNBlogs — 微软拼音注册表 `English Switch Key`：https://www.cnblogs.com/BluePointLilac/p/11688166.html