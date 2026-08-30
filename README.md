# 🪑 坐立提醒 (SitStandReminder)

> **极轻量 · 零外部依赖 · DWM / Direct2D 硬件加速 · 专为 Windows 打造的现代工效健康循环助手**

一款基于 **C++20**、**Pure Win32 API** 与 **Direct2D / DirectWrite** 原生构建的 Windows 桌面健康循环提醒工具。单文件绿色运行，无任何 Electron / Qt / WebView 运行时负担，常态物理内存开销仅 **~2.8 MB**，CPU 占用稳定在 **< 0.005%**，支持 Windows 11 EcoQoS 低功耗调度与 Per-Monitor V2 顶级高分屏自适应。

---

## 🌟 核心特性总览

### 1. 🧘 沉浸式全屏工间操引擎 (Fullscreen Exercise Engine)

- **矢量骨骼动力学动画**：Direct2D 硬件加速自绘，全屏 60 FPS 丝滑渲染，不掉帧、不撕裂；
- **科学颈椎保养操**：
  - 完整涵盖**水平后缩下巴、受控仰角复位 25°、缓慢向左侧拉伸、缓慢向右侧拉伸** 4 大标准康复动作；
  - 配备解剖级侧面/正面五官（眼睛、视线向量束、鼻子、下巴、斜方肌热力带），直观指引动作朝向与发力肌群；
- **20-20-20 科学护眼操**：
  - **法则一**：深空透视光轴隧道与景深光斑，引导凝视 6 米外窗外远景；
  - **法则二**：深度闭目与 4-4-4 呼吸光环（深吸 4s ➔ 屏息 2s ➔ 慢呼 4s）舒缓眼压；
  - **法则三**：全屏大视野 ∞ 轨道彗星视线追踪，带动 6 条眼外肌群舒展；
- **4 种工间操模式**：
  - **综合工间操**：颈椎保养与护眼操无缝连贯组合（推荐 90 秒 / 最少 60 秒）；
  - **科学颈椎操**：专注颈椎肩颈放松（推荐 40 秒 / 最少 32 秒）；
  - **20-20-20 护眼操**：3 阶段各 20 秒标准流程（标准 60 秒）；
  - **极简放空休息**：沉浸式冥想放空（无限制）；
- **安全防误触输入拦截**：全屏休息期间由全局低级键盘钩子（`WH_KEYBOARD_LL`）拦截非必要按键输入，按 `ESC` 键可即刻安全退出。

---

### 2. 🐾 桌面灵动悬浮窗与伴侣系统 (Desktop Floating Widget)

- **DWM 硬件级分层透明窗体 (`WS_EX_LAYERED`)**：Direct2D 32-bit Premultiplied Alpha 亚像素渲染，纯净透明无黑边杂色；
- **4 大专属工效桌面形象**：
  - 🪵 **佛系水豚**：头顶小橘子，工学椅上敲代码呼吸微动；
  - 🐱 **灵动像素猫**：双爪轻快起伏敲击键盘陪伴；
  - 🤖 **赛博小助手**：科技感眨眼与发光天线微光；
  - 🪑 **极简商务**：大画幅标准 90° 人体工学坐姿/站立姿态指示（支持主题深浅色强对比）；
- **智能双模边缘吸附机制 (Smart Dual-Mode Snapping)**：
  - **【贴边常驻 (Snapped Resident, 0~20px)】**：拖至屏幕左右/顶部边缘释放时自动磁力对齐，**100% 完整常驻展示，绝不自动折叠**；
  - **【推入折叠 (Push-to-Collapse)】**：拖拽时故意推向屏幕外边界（越界 > 2px）或鼠标撞墙时激活折叠；鼠标移开 400ms 后以 60 FPS 平滑折叠为 **32px** 灵动胶囊拉手，鼠标轻触拉手即刻 0 延迟平滑滑出展开；
- **外边框周长圆角流光进度条**：从顶部 12 点钟方向顺时针流转，支持 **4 档可选线宽（细线 1.5px / 标准 2.5px / 加粗 3.5px / 醒目极粗 4.5px）**，复杂或亮色壁纸下依然轮廓分明；
- **临界强提醒**：坐姿工作最后 30 秒悬浮窗泛出琥珀/警示红光呼吸晕染，潜意识提示收尾手头工作。

---

### 3. 🐱 系统托盘 RunCat 灵动小猫与动态倒计时 (Tray Engine)

- **4 种托盘呈现风格**：
  - 🖼️ **经典静态图标**：Windows 11 Fluent 风格标准高清应用图标；
  - ⏱️ **动态微缩数字倒计时**：托盘图标实时自绘粗体分钟数字 + 外圈四色自适应进度环（**坐姿翡翠绿 $\rightarrow$ 站立天青蓝 $\rightarrow$ 临界琥珀黄 $\rightarrow$ 休息珊瑚红**）；
  - 🏃 **灵动小猫 RunCat (状态感应)**：5 帧矢量奔跑小猫，坐姿期从容慢跑 (150ms) $\rightarrow$ 临界 5 分钟全力冲刺 (120ms) $\rightarrow$ 站立期欢快弹跳 (140ms) $\rightarrow$ 休息期卷缩打盹带小 `z` 呼吸泡泡 (350ms)；
  - ⚡ **动力小猫 RunCat (CPU 占用率)**：跑速与 Windows 系统当前 CPU 负载（0% ~ 100%）实时硬件联动；
- **锁屏与休眠功耗管理**：锁屏（WTS）或系统睡眠期间自动暂停托盘高频动画定时器，消除后台空转；
- **自绘深浅色现代菜单**：自绘 Fluent 圆角菜单，支持翡翠绿激活圆点、延后 5 分钟、跳过当前阶段、4 大办公周期预设与深浅色模式自动适配。

---

### 4. ⚙️ 设置中心与单实例防抖 (Settings Window)

- **4 大科学办公周期预设（单一事实源 SSOT）**：
  - ⚡ **45m 坐 / 15m 站 / 90s 休息**（经典工效推荐）；
  - ⚡ **50m 坐 / 10m 站 / 90s 休息**（轻量循环）；
  - ⚡ **25m 番茄工作法 / 5m 站立 / 60s 休息**（高效冲刺）；
  - ⚡ **60m 深度攻坚 / 20m 站立 / 90s 休息**（深度沉浸）；
- **深浅主题即时热切换**：DWM 沉浸式暗色标题栏、非客户区 `SWP_FRAMECHANGED` 刷新、全量子控件自绘重绘；
- **键盘回车快捷保存**：在设置中心按 `Enter` 键即可直接保存并应用配置，按 `Esc` 键快速关闭；
- **用户友好提示**：内置 14 个控件的完整 Tooltips 气泡帮助说明，支持提示音开关、开机自启、边缘吸附与窗口置顶开关。

---

### 5. 🛡️ 稳健性与底层硬件特性

- **离座与锁屏时钟防漂移**：
  - 使用 `GetTickCount64` 计算离座物理时间；
  - **离座 > 5 分钟**：自动开启全新工作周期；
  - **短暂离开/锁屏**：精确锚定剩余倒计时，恢复时无缝继续；
- **Per-Monitor V2 高分屏支持**：
  - 清单显式声明 `PerMonitorV2, PerMonitor`；
  - Direct2D 坐标系固定为 96 DPI，彻底消除高分屏下“系统缩放 + D2D 缩放”引起的二次模糊；
  - 多显示器跨屏拖拽与分辨率切换自适应；
- **单实例互斥唤醒**：通过命名 Mutex 保证全局单一实例，重复运行时自动前置唤醒已有实例的设置中心。

---

## 🏗️ 架构与技术栈

| 模块           | 技术选型                                    | 核心优势                                                                           |
| :------------- | :------------------------------------------ | :--------------------------------------------------------------------------------- |
| **编程语言**   | C++20 (MSVC / ISO C++20)                    | 强类型枚举、智能指针、`std::chrono`、RAII 安全                                     |
| **核心状态机** | `StateMachine`                              | 坐姿工作 $\leftrightarrow$ 站立办公 $\leftrightarrow$ 全屏工间操，时钟绝对物理对齐 |
| **图形渲染**   | Direct2D 1.1 / DirectWrite / GDI DIBSection | GPU 硬件抗锯齿加速、DirectWrite 字体缓存池、异形分层透明窗口                       |
| **系统合成**   | DWM (Desktop Window Manager)                | `UpdateLayeredWindow` 硬件 Alpha 混合、沉浸式深浅色标题栏                          |
| **高分屏**     | Per-Monitor DPI Aware v2                    | 动态 DPI 缩放换算（100%、125%、150%、175%、200%）自适应                            |
| **输入拦截**   | Win32 Low-Level Keyboard Hook               | 异步安全消息解耦、`ESC` 紧急解锁                                                   |
| **配置存储**   | Windows Registry (`HKEY_CURRENT_USER`)      | 零额外文件依赖、原子级读写、开机自启原生集成                                       |
| **构建系统**   | CMake 3.20+ / GitHub Actions CI             | 自动编译、单元测试、打包发布并生成 SHA256 校验                                     |

---

## 📂 源码目录结构

```
SitStandReminder/
├── .github/
│   └── workflows/
│       └── build-and-release.yml  # GitHub Actions 自动化编译测试与发布工作流
├── CMakeLists.txt                 # CMake 构建配置文件
├── README.md                      # 项目说明文档
├── resources/
│   ├── app.ico                    # 7级多分辨率原生应用图标 (16x16 ~ 256x256)
│   ├── app.manifest.in            # Common Controls 6.0 与 Per-Monitor DPI v2 清单模板
│   ├── app.rc.in                  # Windows 资源脚本模板 (自动注入 CMake 版本号)
│   └── resource.h                 # 资源 ID 定义头文件
├── src/
│   ├── main.cpp                   # 程序入口、单实例 Mutex、主消息泵与 EcoQoS
│   ├── core/
│   │   ├── AppConstants.hpp       # 全局标识、预设表与单一事实源常量 (SSOT)
│   │   ├── Version.hpp.in         # 版本号 C++ 头文件模板 (自动生成 Version.hpp)
│   │   ├── ConfigManager.hpp      # 配置模型与注册表持久化管理
│   │   ├── ConfigManager.cpp
│   │   ├── StateMachine.hpp       # 核心时钟状态机 (Work / Stand / Rest / Pause)
│   │   └── StateMachine.cpp
│   ├── graphics/
│   │   ├── D2DContext.hpp         # Direct2D/DirectWrite 工厂与字体缓存单例池
│   │   ├── D2DContext.cpp
│   │   ├── DynamicTrayIcon.hpp    # 32-bit ARGB 托盘数字倒计时与 RunCat 生成器
│   │   ├── DynamicTrayIcon.cpp
│   │   ├── EyeExerciseRenderer.hpp # 20-20-20 科学护眼操轨迹与晶状体调节渲染器
│   │   ├── EyeExerciseRenderer.cpp
│   │   ├── MascotRenderer.hpp     # 4 大桌面伴侣 (水豚/像素猫/赛博/商务) 渲染器
│   │   ├── MascotRenderer.cpp
│   │   ├── NeckExerciseRenderer.hpp # 科学颈椎操 2D 骨骼与视线向量渲染器
│   │   └── NeckExerciseRenderer.cpp
│   ├── platform/
│   │   ├── KeyboardHook.hpp       # 全局低级键盘安全拦截钩子
│   │   ├── KeyboardHook.cpp
│   │   ├── ThemeManager.hpp       # 系统深浅色主题检测与 DWM 沉浸式暗色绑定
│   │   └── ThemeManager.cpp
│   └── ui/
│       ├── FloatingWindow.hpp     # 桌面半透明悬浮窗、双模边缘吸附与展开动画
│       ├── FloatingWindow.cpp
│       ├── FullscreenMask.hpp     # 全屏多显示器遮罩工间操提示窗口
│       ├── FullscreenMask.cpp
│       ├── SettingsWindow.hpp     # 设置中心窗口 (自绘深浅色控件与预设切换)
│       ├── SettingsWindow.cpp
│       ├── TrayWindow.hpp         # 系统托盘窗口与自绘上下文菜单
│       └── TrayWindow.cpp
└── tests/
    ├── CMakeLists.txt             # 单元测试构建配置
    └── test_statemachine.cpp      # 14 项核心状态机、时钟锚定、配置与边框线宽单元测试套件
```

---

## 🛠️ 编译与运行指南

### 前置要求

1. **操作系统**：Windows 10 (1809+) 或 Windows 11 (64-bit)
2. **编译器**：Visual Studio 2022 (包含「使用 C++ 的桌面开发」工作负载)
3. **构建工具**：CMake 3.20+
4. **Shell**：PowerShell 7 (推荐) 或 Windows PowerShell

### 本地编译步骤

```powershell
# 1. 克隆代码仓库
git clone https://github.com/CKCat/SitStandReminder.git
cd SitStandReminder

# 2. 生成 Visual Studio 2022 解决方案工程
cmake -B build -G "Visual Studio 17 2022" -A x64

# 3. 编译发布版本 (Release)
cmake --build build --config Release

# 4. 运行全量 14 项单元测试套件
.\build\tests\Release\UnitTests.exe

# 5. 启动坐立提醒
.\build\Release\SitStandReminder.exe
```

---

## 📦 发行与下载

- 最新发行版二进制资产可前往 [GitHub Releases](https://github.com/CKCat/SitStandReminder/releases) 页面下载绿色免安装 ZIP 压缩包；
- 每次向 `main` 分支提交代码均由 GitHub Actions 自动化编译测试并生成最新构建产物。

---

## 💡 灵感来源与致谢 (Acknowledgements)

本项目基于开源项目 [**CKCat/Sedentary-reminder**](https://github.com/CKCat/Sedentary-reminder) 的核心工效健康循环思路与产品理念演进实现。

在继承其优秀交互概念的基础上，本项目采用 **现代 C++20 + Pure Win32 + Direct2D** 进行了 100% 底层纯原生重构，致力于在 Windows 平台上实现 **单文件免安装、极低内存（<3MB）、GPU 硬件加速与零外部依赖** 的极致原生性能体验。

---

## 📄 许可证

Copyright (C) 2026 CKCat. All rights reserved.
