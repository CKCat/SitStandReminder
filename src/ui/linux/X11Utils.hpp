#pragma once

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <string>

namespace X11Utils {

inline void SetWindowUtf8Title(Display* dpy, Window win, const char* utf8Title) {
    if (!dpy || !win || !utf8Title) return;

    Atom utf8String = XInternAtom(dpy, "UTF8_STRING", False);
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom netWmIconName = XInternAtom(dpy, "_NET_WM_ICON_NAME", False);

    size_t len = std::strlen(utf8Title);

    // 1. 设置现代 EWMH 标准 UTF-8 窗口标题与图标标题 (_NET_WM_NAME)
    XChangeProperty(
        dpy, win, netWmName, utf8String, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(utf8Title), static_cast<int>(len)
    );
    XChangeProperty(
        dpy, win, netWmIconName, utf8String, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(utf8Title), static_cast<int>(len)
    );

    // 2. 通过 Xutf8TextListToTextProperty 注入 ICCCM 兼容的标准 WM_NAME
    XTextProperty prop = {};
    char* list[] = { const_cast<char*>(utf8Title) };
    if (Xutf8TextListToTextProperty(dpy, list, 1, XUTF8StringStyle, &prop) == Success) {
        XSetWMName(dpy, win, &prop);
        XSetWMIconName(dpy, win, &prop);
        if (prop.value) {
            XFree(prop.value);
        }
    }
}

inline std::string WStringToUtf8(const std::wstring& wstr) {
    std::string utf8;
    for (wchar_t wc : wstr) {
        uint32_t cp = static_cast<uint32_t>(wc);
        if (cp < 0x80) {
            utf8.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return utf8;
}

inline void SetWindowClass(Display* dpy, Window win, const char* name, const char* className) {
    if (!dpy || !win || !name || !className) return;
    XClassHint hint;
    hint.res_name = const_cast<char*>(name);
    hint.res_class = const_cast<char*>(className);
    XSetClassHint(dpy, win, &hint);
}

inline void SetWindowIcon(Display* dpy, Window win, int width, int height, const uint32_t* argbData) {
    if (!dpy || !win || width <= 0 || height <= 0 || !argbData) return;

    Atom netWmIcon = XInternAtom(dpy, "_NET_WM_ICON", False);
    std::vector<unsigned long> iconData;
    iconData.reserve(2 + width * height);
    iconData.push_back(static_cast<unsigned long>(width));
    iconData.push_back(static_cast<unsigned long>(height));
    for (int i = 0; i < width * height; ++i) {
        iconData.push_back(static_cast<unsigned long>(argbData[i]));
    }

    XChangeProperty(
        dpy, win, netWmIcon, XA_CARDINAL, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(iconData.data()),
        static_cast<int>(iconData.size())
    );
}

} // namespace X11Utils

#endif
