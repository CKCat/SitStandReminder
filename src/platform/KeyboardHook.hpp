#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>

#define WM_APP_REST_ESCAPE (WM_APP + 101)

class KeyboardHook {
public:
    static KeyboardHook& Instance() {
        static KeyboardHook instance;
        return instance;
    }

    bool Install(HWND hNotifyTarget = nullptr);
    void Uninstall();
    bool IsInstalled() const { return m_hHook != nullptr; }

    using EscapeCallback = std::function<void()>;
    void SetOnEscapePressed(EscapeCallback cb) { m_onEscape = std::move(cb); }

private:
    KeyboardHook() = default;
    ~KeyboardHook() { Uninstall(); }

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK m_hHook = nullptr;
    HWND m_hNotifyTarget = nullptr;
    EscapeCallback m_onEscape;
};

