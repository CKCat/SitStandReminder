#include "KeyboardHook.hpp"

bool KeyboardHook::Install(HWND hNotifyTarget) {
    if (m_hHook) return true;
    m_hNotifyTarget = hNotifyTarget;
    m_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    return m_hHook != nullptr;
}

void KeyboardHook::Uninstall() {
    if (m_hHook) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = nullptr;
        m_hNotifyTarget = nullptr;
    }
}

LRESULT CALLBACK KeyboardHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        auto* kbdStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kbdStruct) {
            // 1. 放行 ESC 键并异步触发紧急退出（避免在 Hook 线程上下文中同步销毁窗口引发重入）
            if (kbdStruct->vkCode == VK_ESCAPE) {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    auto& inst = KeyboardHook::Instance();
                    if (inst.m_hNotifyTarget) {
                        PostMessageW(inst.m_hNotifyTarget, WM_APP_REST_ESCAPE, 0, 0);
                    } else if (inst.m_onEscape) {
                        inst.m_onEscape();
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            // 2. 防御性放行系统修饰键的 KEYUP 事件，防止全屏退出后全局键盘按键状态（Async Key State）卡死
            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                DWORD vk = kbdStruct->vkCode;
                if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
                    vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                    vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
                    vk == VK_LWIN || vk == VK_RWIN) {
                    return CallNextHookEx(nullptr, nCode, wParam, lParam);
                }
            }

            // 拦截常规按键（KeyDown / KeyUp / Char）
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

