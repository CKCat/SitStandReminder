#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define WM_APP_REST_ESCAPE (WM_APP + 101)
#endif

#include <functional>

class KeyboardHook {
public:
    static KeyboardHook& Instance() {
        static KeyboardHook instance;
        return instance;
    }

#ifdef _WIN32
    bool Install(HWND hNotifyTarget = nullptr);
#else
    bool Install(void* hNotifyTarget = nullptr);
#endif
    void Uninstall();
    bool IsInstalled() const;

    using EscapeCallback = std::function<void()>;
    void SetOnEscapePressed(EscapeCallback cb) { m_onEscape = std::move(cb); }
    void TriggerEscape() { if (m_onEscape) m_onEscape(); }

private:
    KeyboardHook() = default;
    ~KeyboardHook() { Uninstall(); }

#ifdef _WIN32
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    HHOOK m_hHook = nullptr;
    HWND m_hNotifyTarget = nullptr;
#else
    bool m_installed = false;
#endif
    EscapeCallback m_onEscape;
};
