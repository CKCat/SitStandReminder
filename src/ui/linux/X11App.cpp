#include "X11App.hpp"

#ifndef _WIN32

#include <poll.h>
#include <chrono>
#include <iostream>
#include <dlfcn.h>
#include <dbus/dbus.h>
#include <algorithm>
#include <X11/Xresource.h>
#include "../../platform/SingleInstance.hpp"
#include "StatusNotifierItemLinux.hpp"

struct XineramaScreenInfoRaw {
    int screen_number;
    short x_org;
    short y_org;
    short width;
    short height;
};
typedef Bool (*pfn_XineramaIsActive)(Display*);
typedef XineramaScreenInfoRaw* (*pfn_XineramaQueryScreens)(Display*, int*);

bool X11App::Initialize() {
    if (m_display) return true;
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "[SitStandReminder] Failed to open X11 Display!" << std::endl;
        return false;
    }

    m_screen = DefaultScreen(m_display);

    // 优先匹配 32-bit ARGB TrueColor 视觉模式，实现原生硬件级 Alpha 透明混合
    XVisualInfo vinfo;
    if (XMatchVisualInfo(m_display, m_screen, 32, TrueColor, &vinfo)) {
        m_visual32 = vinfo.visual;
        m_depth32 = 32;
        m_colormap32 = XCreateColormap(m_display, RootWindow(m_display, m_screen), m_visual32, AllocNone);
    } else {
        m_visual32 = DefaultVisual(m_display, m_screen);
        m_depth32 = DefaultDepth(m_display, m_screen);
        m_colormap32 = DefaultColormap(m_display, m_screen);
    }

    InitializeDBusPowerMonitoring();

    return true;
}

void X11App::Uninitialize() {
    if (m_dbusSystemConn) {
        dbus_connection_close(static_cast<DBusConnection*>(m_dbusSystemConn));
        dbus_connection_unref(static_cast<DBusConnection*>(m_dbusSystemConn));
        m_dbusSystemConn = nullptr;
    }
    if (m_dbusSessionConn) {
        dbus_connection_close(static_cast<DBusConnection*>(m_dbusSessionConn));
        dbus_connection_unref(static_cast<DBusConnection*>(m_dbusSessionConn));
        m_dbusSessionConn = nullptr;
    }

    if (m_display) {
        if (m_depth32 == 32 && m_colormap32) {
            XFreeColormap(m_display, m_colormap32);
            m_colormap32 = 0;
        }
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

Atom X11App::GetAtom(const std::string& name) {
    if (!m_display) return 0;
    auto it = m_atoms.find(name);
    if (it != m_atoms.end()) return it->second;
    Atom atom = XInternAtom(m_display, name.c_str(), False);
    m_atoms[name] = atom;
    return atom;
}

std::vector<ScreenGeometry> X11App::GetScreenGeometries() {
    std::vector<ScreenGeometry> screens;
    if (!m_display) return screens;

    static void* s_xineramaHandle = nullptr;
    if (!s_xineramaHandle) {
        s_xineramaHandle = dlopen("libXinerama.so.1", RTLD_LAZY);
        if (!s_xineramaHandle) {
            s_xineramaHandle = dlopen("libXinerama.so", RTLD_LAZY);
        }
    }

    if (s_xineramaHandle) {
        auto pIsActive = reinterpret_cast<pfn_XineramaIsActive>(dlsym(s_xineramaHandle, "XineramaIsActive"));
        auto pQueryScreens = reinterpret_cast<pfn_XineramaQueryScreens>(dlsym(s_xineramaHandle, "XineramaQueryScreens"));
        if (pIsActive && pQueryScreens && pIsActive(m_display)) {
            int count = 0;
            XineramaScreenInfoRaw* xscreens = pQueryScreens(m_display, &count);
            if (xscreens && count > 0) {
                for (int i = 0; i < count; ++i) {
                    ScreenGeometry geom;
                    geom.x = xscreens[i].x_org;
                    geom.y = xscreens[i].y_org;
                    geom.width = xscreens[i].width;
                    geom.height = xscreens[i].height;
                    geom.isPrimary = (geom.x == 0 && geom.y == 0) || (i == 0);
                    screens.push_back(geom);
                }
                XFree(xscreens);
            }
        }
    }

    if (screens.empty()) {
        ScreenGeometry geom;
        geom.x = 0;
        geom.y = 0;
        geom.width = DisplayWidth(m_display, m_screen);
        geom.height = DisplayHeight(m_display, m_screen);
        geom.isPrimary = true;
        screens.push_back(geom);
    }

    return screens;
}

ScreenGeometry X11App::GetPrimaryScreen() {
    auto screens = GetScreenGeometries();
    for (const auto& s : screens) {
        if (s.isPrimary) return s;
    }
    if (!screens.empty()) return screens[0];
    return ScreenGeometry{0, 0, DisplayWidth(m_display, m_screen), DisplayHeight(m_display, m_screen), true};
}

ScreenGeometry X11App::GetScreenForPoint(int x, int y) {
    auto screens = GetScreenGeometries();
    for (const auto& s : screens) {
        if (x >= s.x && x < s.x + s.width &&
            y >= s.y && y < s.y + s.height) {
            return s;
        }
    }
    return GetPrimaryScreen();
}

float X11App::GetDisplayDpiScale() {
    if (m_cachedDpiScale > 0.1f) {
        return m_cachedDpiScale;
    }

    float detectedDpi = 0.0f;

    // 1. 优先读取桌面环境变量 GDK_SCALE 与 QT_SCALE_FACTOR (高优先级用户覆盖配置)
    const char* gdkScale = getenv("GDK_SCALE");
    if (gdkScale) {
        try {
            float s = std::stof(gdkScale);
            if (s >= 0.5f && s <= 5.0f) {
                detectedDpi = s * 96.0f;
            }
        } catch (...) {}
    }
    if (detectedDpi <= 0.0f) {
        const char* qtScale = getenv("QT_SCALE_FACTOR");
        if (qtScale) {
            try {
                float s = std::stof(qtScale);
                if (s >= 0.5f && s <= 5.0f) {
                    detectedDpi = s * 96.0f;
                }
            } catch (...) {}
        }
    }

    // 2. 尝试从 X ResourceManager 读取 Xft.dpi 属性
    if (detectedDpi <= 0.0f && m_display) {
        char* resourceManager = XResourceManagerString(m_display);
        if (resourceManager) {
            XrmInitialize();
            XrmDatabase db = XrmGetStringDatabase(resourceManager);
            if (db) {
                XrmValue value;
                char* type = nullptr;
                if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value)) {
                    if (value.addr) {
                        try {
                            float dpiVal = std::stof(value.addr);
                            if (dpiVal > 30.0f && dpiVal < 1000.0f) {
                                detectedDpi = dpiVal;
                            }
                        } catch (...) {}
                    }
                }
                XrmDestroyDatabase(db);
            }
        }
    }

    // 3. 尝试从 X11 物理屏幕尺寸 DisplayWidthMM 计算
    if (detectedDpi <= 0.0f && m_display) {
        int widthPx = DisplayWidth(m_display, m_screen);
        int widthMm = DisplayWidthMM(m_display, m_screen);
        if (widthMm > 50 && widthPx > 100) {
            float calculatedDpi = (static_cast<float>(widthPx) * 25.4f) / static_cast<float>(widthMm);
            if (calculatedDpi >= 60.0f && calculatedDpi <= 400.0f) {
                detectedDpi = calculatedDpi;
            }
        }
    }

    // 4. 计算缩放因子 (以标准 96 DPI 为基准)，异常则兜底 1.0f
    float scale = (detectedDpi > 0.0f) ? (detectedDpi / 96.0f) : 1.0f;
    scale = (std::clamp)(scale, 0.5f, 4.0f);

    m_cachedDpiScale = scale;
    return m_cachedDpiScale;
}

void X11App::InitializeDBusPowerMonitoring() {
    DBusError err;
    dbus_error_init(&err);

    // 1. System Bus 监听 PrepareForSleep
    DBusConnection* sysConn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (sysConn && !dbus_error_is_set(&err)) {
        dbus_connection_set_exit_on_disconnect(sysConn, FALSE);
        dbus_bus_add_match(sysConn, "type='signal',interface='org.freedesktop.login1.Manager',member='PrepareForSleep'", &err);
        dbus_connection_flush(sysConn);
        m_dbusSystemConn = sysConn;
    }
    dbus_error_free(&err);

    // 2. Session Bus 监听 ScreenSaver.ActiveChanged
    dbus_error_init(&err);
    DBusConnection* sessConn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (sessConn && !dbus_error_is_set(&err)) {
        dbus_connection_set_exit_on_disconnect(sessConn, FALSE);
        dbus_bus_add_match(sessConn, "type='signal',interface='org.freedesktop.ScreenSaver',member='ActiveChanged'", &err);
        dbus_connection_flush(sessConn);
        m_dbusSessionConn = sessConn;
    }
    dbus_error_free(&err);
}

void X11App::ProcessDBusPowerEvents() {
    auto processConn = [this](DBusConnection* conn) {
        if (!conn) return;
        dbus_connection_read_write(conn, 0);
        DBusMessage* msg = nullptr;
        while ((msg = dbus_connection_pop_message(conn)) != nullptr) {
            if (dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")) {
                dbus_bool_t sleepActive = FALSE;
                DBusError err;
                dbus_error_init(&err);
                if (dbus_message_get_args(msg, &err, DBUS_TYPE_BOOLEAN, &sleepActive, DBUS_TYPE_INVALID)) {
                    if (m_powerCallback) {
                        m_powerCallback(sleepActive != FALSE);
                    }
                }
                dbus_error_free(&err);
            } else if (dbus_message_is_signal(msg, "org.freedesktop.ScreenSaver", "ActiveChanged")) {
                dbus_bool_t screenSaverActive = FALSE;
                DBusError err;
                dbus_error_init(&err);
                if (dbus_message_get_args(msg, &err, DBUS_TYPE_BOOLEAN, &screenSaverActive, DBUS_TYPE_INVALID)) {
                    if (m_powerCallback) {
                        m_powerCallback(screenSaverActive != FALSE);
                    }
                }
                dbus_error_free(&err);
            }
            dbus_message_unref(msg);
        }
    };

    if (m_dbusSystemConn) processConn(static_cast<DBusConnection*>(m_dbusSystemConn));
    if (m_dbusSessionConn) processConn(static_cast<DBusConnection*>(m_dbusSessionConn));
}

void X11App::RegisterWindow(Window win, EventHandler handler) {
    m_windowHandlers[win] = std::move(handler);
}

void X11App::UnregisterWindow(Window win) {
    m_windowHandlers.erase(win);
}

void X11App::AddAnimationCallback(void* key, AnimCallback cb) {
    m_animCallbacks[key] = std::move(cb);
}

void X11App::RemoveAnimationCallback(void* key) {
    m_animCallbacks.erase(key);
}

void X11App::ExitEventLoop() {
    m_running = false;
}

void X11App::RunEventLoop(std::function<void()> onSecondTick) {
    if (!m_display) return;
    m_running = true;

    int x11Fd = ConnectionNumber(m_display);
    auto lastTickTime = std::chrono::steady_clock::now();
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_running) {
        if (SingleInstance::Instance().CheckAndResetWakeRequested()) {
            SingleInstance::Instance().TriggerWake();
        }

        // 1. 处理所有积压的 X11 窗口事件
        while (XPending(m_display) > 0) {
            XEvent ev;
            XNextEvent(m_display, &ev);
            auto it = m_windowHandlers.find(ev.xany.window);
            if (it != m_windowHandlers.end() && it->second) {
                it->second(ev);
            }
        }

        // 2. 派发 D-Bus 现代托盘 (StatusNotifierItem) 协议消息与电源锁屏事件
        StatusNotifierItemLinux::Instance().ProcessEvents();
        ProcessDBusPowerEvents();

        auto now = std::chrono::steady_clock::now();

        // 3. 60 FPS (~16ms) 动画帧调度
        auto frameElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();
        if (frameElapsed >= 16) {
            lastFrameTime = now;
            auto callbacks = m_animCallbacks;
            for (auto& pair : callbacks) {
                if (pair.second) {
                    pair.second();
                }
            }
            XFlush(m_display);
        }

        // 4. 1 秒精准业务时钟
        auto tickElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickTime).count();
        if (tickElapsed >= 1000) {
            lastTickTime = now;
            if (onSecondTick) {
                onSecondTick();
            }
            XFlush(m_display);
        }

        // 5. 智能节能休眠等待
        int timeoutMs = 16;
        if (m_animCallbacks.empty()) {
            auto remainToTick = 1000 - tickElapsed;
            timeoutMs = std::clamp(static_cast<int>(remainToTick), 10, 100);
        }

        struct pollfd pfds[4];
        int numFds = 1;
        pfds[0].fd = x11Fd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;

        int dbusFd = StatusNotifierItemLinux::Instance().GetConnectionSocket();
        if (dbusFd >= 0) {
            pfds[numFds].fd = dbusFd;
            pfds[numFds].events = POLLIN;
            pfds[numFds].revents = 0;
            numFds++;
        }

        if (m_dbusSystemConn) {
            int sysFd = -1;
            if (dbus_connection_get_unix_fd(static_cast<DBusConnection*>(m_dbusSystemConn), &sysFd) && sysFd >= 0) {
                pfds[numFds].fd = sysFd;
                pfds[numFds].events = POLLIN;
                pfds[numFds].revents = 0;
                numFds++;
            }
        }

        if (m_dbusSessionConn) {
            int sessFd = -1;
            if (dbus_connection_get_unix_fd(static_cast<DBusConnection*>(m_dbusSessionConn), &sessFd) && sessFd >= 0) {
                pfds[numFds].fd = sessFd;
                pfds[numFds].events = POLLIN;
                pfds[numFds].revents = 0;
                numFds++;
            }
        }

        poll(pfds, numFds, timeoutMs);
    }
}

#endif
