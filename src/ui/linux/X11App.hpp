#pragma once

#ifndef _WIN32

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

struct ScreenGeometry {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool isPrimary = false;
};

class X11App {
public:
    static X11App& Instance() {
        static X11App inst;
        return inst;
    }

    bool Initialize();
    void Uninitialize();

    Display* GetDisplay() const { return m_display; }
    int GetScreen() const { return m_screen; }
    Visual* GetVisual32() const { return m_visual32; }
    Colormap GetColormap32() const { return m_colormap32; }
    int GetDepth32() const { return m_depth32; }

    Atom GetAtom(const std::string& name);

    // 多显示器拓扑支持
    std::vector<ScreenGeometry> GetScreenGeometries();
    ScreenGeometry GetPrimaryScreen();
    ScreenGeometry GetScreenForPoint(int x, int y);

    // DPI 探测支持 (DEF-06)
    float GetDisplayDpiScale();
    void InvalidateDpiCache() { m_cachedDpiScale = -1.0f; }

    using EventHandler = std::function<void(const XEvent&)>;
    void RegisterWindow(Window win, EventHandler handler);
    void UnregisterWindow(Window win);

    using AnimCallback = std::function<void()>;
    void AddAnimationCallback(void* key, AnimCallback cb);
    void RemoveAnimationCallback(void* key);

    // 系统电源休眠/锁屏状态变化回调
    using PowerStateCallback = std::function<void(bool isSuspendOrLock)>;
    void SetPowerStateCallback(PowerStateCallback cb) { m_powerCallback = std::move(cb); }

    void RunEventLoop(std::function<void()> onSecondTick);
    void ExitEventLoop();

private:
    X11App() = default;
    ~X11App() { Uninitialize(); }

    void InitializeDBusPowerMonitoring();
    void ProcessDBusPowerEvents();

    Display* m_display = nullptr;
    int m_screen = 0;
    Visual* m_visual32 = nullptr;
    Colormap m_colormap32 = 0;
    int m_depth32 = 24;

    bool m_running = false;
    std::unordered_map<Window, EventHandler> m_windowHandlers;
    std::unordered_map<void*, AnimCallback> m_animCallbacks;
    std::unordered_map<std::string, Atom> m_atoms;
    PowerStateCallback m_powerCallback;

    void* m_dbusSystemConn = nullptr;
    void* m_dbusSessionConn = nullptr;
    float m_cachedDpiScale = -1.0f;
};

#endif
