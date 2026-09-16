#pragma once

#ifndef _WIN32
#include <string>
#include <functional>
#include <memory>

struct DBusConnection;

class StatusNotifierItemLinux {
public:
    static StatusNotifierItemLinux& Instance() {
        static StatusNotifierItemLinux inst;
        return inst;
    }

    bool Initialize(
        std::function<void(int x, int y)> onContextMenu,
        std::function<void(int x, int y)> onActivate
    );
    void Shutdown();

    void ProcessEvents();
    void SetTooltip(const std::string& title, const std::string& description);
    void SetIconName(const std::string& iconName);
    void SetIconPixmap(int width, int height, const uint32_t* argbData);
    void SetLabel(const std::string& label);

    void SetMenuCommandHandler(std::function<void(uint32_t cmdId)> onMenuCommand) {
        m_onMenuCommand = std::move(onMenuCommand);
    }
    int GetConnectionSocket() const;

    const std::string& GetTooltipTitle() const { return m_tooltipTitle; }
    const std::string& GetTooltipDesc() const { return m_tooltipDesc; }
    static struct DBusMessage* TestHandleGetProperty(struct DBusMessage* msg, const char* iface, const char* prop, StatusNotifierItemLinux* self) {
        return HandleGetProperty(msg, iface, prop, self);
    }
    static struct DBusMessage* TestHandleDBusMenuMethod(struct DBusMessage* msg, StatusNotifierItemLinux* self) {
        return HandleDBusMenuMethod(msg, self);
    }
    static int TestMessageFilter(struct DBusConnection* conn, struct DBusMessage* msg, void* user_data) {
        return MessageFilter(conn, msg, user_data);
    }

    bool IsAvailable() const { return m_registered; }

private:
    StatusNotifierItemLinux() = default;
    ~StatusNotifierItemLinux() { Shutdown(); }

    DBusConnection* m_conn = nullptr;
    std::string m_serviceName;
    std::string m_objectPath = "/StatusNotifierItem";
    std::string m_menuPath = "/MenuBar";
    bool m_registered = false;

    std::string m_title = "坐立提醒";
    std::string m_tooltipTitle = "坐立提醒";
    std::string m_tooltipDesc = "健康办公工间操伴侣";
    std::string m_iconName = "sitstandreminder";
    std::string m_iconThemePath;
    std::string m_label;

    int m_pixmapWidth = 0;
    int m_pixmapHeight = 0;
    std::vector<uint8_t> m_pixmapBytes;

    std::function<void(int x, int y)> m_onContextMenu;
    std::function<void(int x, int y)> m_onActivate;
    std::function<void(uint32_t cmdId)> m_onMenuCommand;

    static struct DBusMessage* HandleGetProperty(struct DBusMessage* msg, const char* iface, const char* prop, StatusNotifierItemLinux* self);
    static struct DBusMessage* HandleGetAllProperties(struct DBusMessage* msg, const char* iface, StatusNotifierItemLinux* self);
    static struct DBusMessage* HandleDBusMenuMethod(struct DBusMessage* msg, StatusNotifierItemLinux* self);
    static int MessageFilter(DBusConnection* conn, struct DBusMessage* msg, void* user_data);
    bool RegisterWithWatcher();
    void InitIconThemePath();
};
#endif
