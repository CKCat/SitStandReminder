# 🪑 久坐健康提醒 (Sedentary Reminder Native)

> **极轻量 · 零依赖 · 硬件加速 · 专为 Windows 打造的现代工效健康循环助手**

一款基于 **C++20**、**Win32 API** 与 **Direct2D / DirectWrite** 纯原生构建的 Windows 桌面健康循环提醒软件。单文件体积仅 **~177 KB**，常态物理内存开销仅 **~2.68 MB**（相比 Electron 节省 99%），CPU 占用稳定在 **< 0.005%**，支持 Windows 11 EcoQoS 效率调度，拒绝任何 Electron / Webview 等臃肿运行时的资源浪费。

---

## 🌟 核心特性总览

### 1. 🧘 沉浸式全屏工间操引擎 (Fullscreen Exercise Engine)

- **矢量骨骼动力学动画**：采用 Direct2D 硬件加速自绘，全屏 60 FPS 流畅渲染；
- **科学颈椎操**：
  - 完整涵盖**仰头、低头、左右转头、侧屈拉伸**等标准康复动作；
  - **高占比动作演示**，仰头时配备面部五官（眼睛、鼻子）微细节，直观指示转头与仰头朝向；
- **20-20-20 科学护眼操**：
  - 标准「8」字眼球环形转动轨迹；
  - 远近焦点深度切换视力调节与眼睑闭合深呼吸放松；
- **综合工间操**：颈椎保养与护眼操无缝连贯组合；
- **极简放空模式**：沉浸式深呼吸光环与渐变冥想粒子；
- **工效学时长约束法则**：综合操最少 60 秒（满血推荐 90 秒），20-20-20 科学护眼操标准 60 秒（3 阶段各 20 秒），科学颈椎操最少 32 秒（满血推荐 40 秒），极简放空无限制；
- **安全防误触拦截**：全屏休息期间可通过全局低级键盘钩子安全拦截常规输入，按 `ESC` 键可随时紧急退出解锁。

---

### 2. 🐾 桌面灵动悬浮窗与伴侣系统 (Desktop Floating Widget)

- **硬件级异形分层透明窗体 (`WS_EX_LAYERED`)**：Direct2D 亚像素渲染，锐利抗锯齿；
- **4 大专属桌面陪伴形象**：
  - 🪵 **佛系水豚**：头顶橘子呼吸微动，治愈办公焦虑；
  - 🐱 **灵动像素猫**：双爪轻快敲击键盘敲代码；
  - 🤖 **赛博小助手**：科技感眨眼与天线微光闪烁；
  - 🪑 **极简商务**：人体工学健康坐姿实时指示；
- **智能分层边缘吸附机制 (Smart Dual-Mode Snapping)**：
  - **【贴边常驻 (Snapped Resident, 0~18px)】**：拖至屏幕边缘自动磁力吸附对齐，**100% 完整常驻展示，绝不自动折叠**，满足角落时刻看时间的需求；
  - **【推入折叠 (Push-to-Collapse)】**：拖拽时故意推向屏幕外边界（越界 > 6px）或鼠标撞墙时激活隐藏模式；鼠标离开 400ms 后以 60 FPS 平滑折叠为 **32px** 灵动小拉手，鼠标轻触拉手即刻 0 延迟平滑滑出展开；
- **临界强提醒**：倒计时最后 30 秒悬浮窗泛出警示红光呼吸晕染，潜意识提示收尾手头工作。

---

### 3. 🐱 系统托盘 RunCat 灵动小猫与动态倒计时 (Tray Engine)

- **4 种托盘呈现风格**：
  - 🖼️ **经典静态图标**：Windows 11 Fluent 风格标准应用图标；
  - ⏱️ **动态微缩数字倒计时**：托盘图标实时自绘粗体分钟数字 + 外圈四色自适应进度环（**工作翡翠绿 $\rightarrow$ 站立天青蓝 $\rightarrow$ 临界琥珀黄 $\rightarrow$ 休息珊瑚红**），即使隐藏浮窗也能余光掌握时间；
  - 🏃 **灵动小猫 RunCat (状态感应)**：5 帧矢量奔跑小猫，专注期从容慢跑 (120ms) $\rightarrow$ 临界 5 分钟全力冲刺 (60ms) $\rightarrow$ 站立期欢快弹跳 (90ms) $\rightarrow$ 休息期卷缩打盹带小 `z` 呼吸泡泡；
  - ⚡ **动力小猫 RunCat (CPU占用率)**：跑速与 Windows 系统当前 CPU 负载（0% ~ 100%）实时联动；
- **GDI 句柄严格单槽置换**：每次更新严谨销毁旧句柄（`DestroyIcon`），彻底杜绝 Windows GDI Handle 泄漏；
- **统一自绘深浅色菜单**：右键菜单支持翡翠绿激活圆点、延后 5 分钟、跳过、四大办公周期预设与深浅模式自动适配。

---

### 4. ⚙️ 设置中心与现代化 Win32 UI (Settings Window)

- **4 大科学办公周期预设**：
  - ⚡ **45m 专注 / 15m 站立 / 60s 休息**（经典工效推荐）；
  - ⚡ **50m 专注 / 10m 站立 / 60s 休息**（轻量循环）；
  - ⚡ **25m 番茄工作法 / 5m 站立 / 30s 休息**（高效冲刺）；
  - ⚡ **60m 深度办公 / 20m 站立 / 60s 休息**（深度沉浸）；
- **深浅主题即时热切换**：DWM 沉浸式暗色标题栏、非客户区 `SWP_FRAMECHANGED` 刷新、全量子控件自绘重绘；
- **Win32 消息递归防重入安全锁**：根治 `SetWindowTheme` 引起的 `0xC00000FD` (Stack Overflow) 消息死循环崩溃；
- **友好应用反馈**：点击「保存并应用」保持窗口打开，并展示「✓ 已应用」动画反馈；
- **双击与右键统一**：双击托盘图标或在浮窗右键菜单中均可直接唤出设置中心。

---

### 5. 🎨 原生 Windows 资源体系 (Windows Native Assets)

- **7 级完整分辨率原生图标 (`app.ico`)**：
  - 涵盖 `256x256`（PNG 压缩流，适配 Windows 资源管理器特大图标与 4K 高分屏）、`128x128`、`64x64`、`48x48`、`32x32`、`24x24`、`16x16`；
- **原生资源脚本与版本信息 (`app.rc` & `resource.h`)**：
  - `IDI_APP_ICON (101)` 最低序号资源，Windows 资源管理器、任务栏、Alt+Tab 任务切换器自动识别；
  - 内置标准 Windows 语义化版本信息块（`VS_VERSION_INFO`，版权、公司名、产品名与版本 `1.0.0.0`）。

---

## 🏗️ 架构与技术栈

| 模块           | 技术选型                      | 说明                                                                                         |
| :------------- | :---------------------------- | :------------------------------------------------------------------------------------------- |
| **编程语言**   | C++20                         | 强类型枚举、Lambda 捕获、智能指针                                                            |
| **核心状态机** | `StateMachine`                | 坐姿工作 $\leftrightarrow$ 站立办公 $\leftrightarrow$ 全屏休息工效循环，支持时钟绝对物理对齐 |
| **图形渲染**   | Direct2D / DirectWrite / GDI+ | 硬件级抗锯齿、亚像素字体排版、异形分层透明窗口                                               |
| **系统主题**   | Windows DWM & UXTheme API     | 完美适配 Windows 10/11 系统深色与浅色模式                                                    |
| **高分屏支持** | Per-Monitor DPI Aware v2      | 多显示器动态拖拽与缩放比（100%、125%、150%、200%）自适应                                     |
| **键盘钩子**   | Win32 Low-Level Keyboard Hook | 全屏休息期间输入安全拦截与 `ESC` 紧急解锁                                                    |
| **构建系统**   | CMake 3.20+ / MSVC            | 生成独立静态可执行文件，无第三方动态链接库依赖                                               |

---

## 📂 源码目录结构

```
native/
├── CMakeLists.txt              # CMake 构建配置文件
├── resources/
│   ├── app.ico                 # 7级多分辨率 Windows 原生应用图标
│   ├── app.manifest            # Common Controls 6.0 与 Per-Monitor DPI v2 清单
│   ├── app.rc                  # Windows 资源脚本
│   └── resource.h              # 资源 ID 定义头文件
├── src/
│   ├── main.cpp                # 应用程序入口、消息循环与实例单例互斥锁
│   ├── core/
│   │   ├── ConfigManager.hpp   # 用户配置管理与 Windows 注册表持久化
│   │   ├── ConfigManager.cpp
│   │   ├── StateMachine.hpp    # 核心工效时钟状态机 (Work/Stand/Rest/Pause)
│   │   └── StateMachine.cpp
│   ├── graphics/
│   │   ├── D2DContext.hpp      # Direct2D/DirectWrite 工厂与字体缓存单例
│   │   ├── D2DContext.cpp
│   │   ├── NeckExerciseRenderer.hpp # 科学颈椎操 2D 骨骼渲染器
│   │   ├── NeckExerciseRenderer.cpp
│   │   ├── EyeExerciseRenderer.hpp  # 20-20-20 护眼操轨迹与晶状体调节渲染器
│   │   ├── EyeExerciseRenderer.cpp
│   │   ├── MascotRenderer.hpp       # 4 大桌面伴侣 (水豚/像素猫/赛博/商务) 动画渲染器
│   │   ├── MascotRenderer.cpp
│   │   ├── DynamicTrayIcon.hpp      # 32-bit ARGB 托盘数字倒计时与 RunCat 动画小猫生成器
│   │   └── DynamicTrayIcon.cpp
│   ├── platform/
│   │   ├── ThemeManager.hpp    # Windows 系统深浅色主题检测与 DWM 沉浸式暗色绑定
│   │   ├── ThemeManager.cpp
│   │   ├── KeyboardHook.hpp    # 全局键盘输入安全拦截钩子
│   │   └── KeyboardHook.cpp
│   └── ui/
│       ├── FloatingWindow.hpp  # 桌面 Direct2D 悬浮窗、双模贴边与平滑展开动画
│       ├── FloatingWindow.cpp
│       ├── FullscreenMask.hpp  # 全屏遮罩工间操提示窗口
│       ├── FullscreenMask.cpp
│       ├── SettingsWindow.hpp  # 设置中心窗口 (自绘深浅 ComboBox/Button/Checkbox)
│       ├── SettingsWindow.cpp
│       ├── TrayWindow.hpp      # 系统托盘窗口与自绘上下文菜单
│       └── TrayWindow.cpp
└── tests/
    ├── CMakeLists.txt          # 单元测试构建配置
    └── test_statemachine.cpp   # 8 大核心状态机与配置单元测试套件
```

---

## 🛠️ 编译与运行指南

### 前置要求

1. **Windows 10 / 11** (64-bit)
2. **Visual Studio 2022** (包含「使用 C++ 的桌面开发」工作负载)
3. **CMake 3.20+**
4. **PowerShell 7** (推荐)

### 编译步骤

```powershell
# 1. 进入原生工程目录并生成构建工程
cd native
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. 编译发布版 (Release)
cmake --build build --config Release

# 3. 运行全部单元测试 (8/8 Suites Passed)
& "build/tests/Release/UnitTests.exe"

# 4. 启动应用程序
& "build/Release/SedentaryReminder.exe"
```

---

## 📄 许可证

Copyright (C) 2026 CKCat. All rights reserved.
