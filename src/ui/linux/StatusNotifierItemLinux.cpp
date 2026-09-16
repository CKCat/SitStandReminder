#ifndef _WIN32
#include "StatusNotifierItemLinux.hpp"
#include <dbus/dbus.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <atomic>

static void AppendStringVariant(DBusMessageIter* iter, const char* str) {
    DBusMessageIter varIter;
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &varIter);
    dbus_message_iter_append_basic(&varIter, DBUS_TYPE_STRING, &str);
    dbus_message_iter_close_container(iter, &varIter);
}

static void AppendBoolVariant(DBusMessageIter* iter, dbus_bool_t val) {
    DBusMessageIter varIter;
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "b", &varIter);
    dbus_message_iter_append_basic(&varIter, DBUS_TYPE_BOOLEAN, &val);
    dbus_message_iter_close_container(iter, &varIter);
}

static void AppendDictEntry(DBusMessageIter* dictIter, const char* key, const char* val) {
    DBusMessageIter entryIter;
    dbus_message_iter_open_container(dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
    dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &key);
    AppendStringVariant(&entryIter, val);
    dbus_message_iter_close_container(dictIter, &entryIter);
}

static void AppendDictEntryBool(DBusMessageIter* dictIter, const char* key, dbus_bool_t val) {
    DBusMessageIter entryIter;
    dbus_message_iter_open_container(dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
    dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &key);
    AppendBoolVariant(&entryIter, val);
    dbus_message_iter_close_container(dictIter, &entryIter);
}

static void AppendToolTipVariant(DBusMessageIter* iter, const char* iconName, const char* title, const char* desc) {
    DBusMessageIter varIter;
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &varIter);

    DBusMessageIter structIter;
    dbus_message_iter_open_container(&varIter, DBUS_TYPE_STRUCT, nullptr, &structIter);

    // 1. icon_name (s)
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &iconName);

    // 2. icon_data (a(iiay))
    DBusMessageIter arrayIter;
    dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "(iiay)", &arrayIter);
    dbus_message_iter_close_container(&structIter, &arrayIter);

    // 3. title (s)
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &title);

    // 4. description (s)
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_STRING, &desc);

    dbus_message_iter_close_container(&varIter, &structIter);
    dbus_message_iter_close_container(iter, &varIter);
}

static void AppendDictEntryToolTip(DBusMessageIter* dictIter, const char* key, const char* iconName, const char* title, const char* desc) {
    DBusMessageIter entryIter;
    dbus_message_iter_open_container(dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
    dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &key);
    AppendToolTipVariant(&entryIter, iconName, title, desc);
    dbus_message_iter_close_container(dictIter, &entryIter);
}

static void AppendIconPixmapVariant(DBusMessageIter* iter, int width, int height, const uint8_t* bytes, size_t byteCount) {
    DBusMessageIter varIter;
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "a(iiay)", &varIter);

    DBusMessageIter arrayIter;
    dbus_message_iter_open_container(&varIter, DBUS_TYPE_ARRAY, "(iiay)", &arrayIter);

    if (width > 0 && height > 0 && bytes != nullptr && byteCount >= static_cast<size_t>(width * height * 4)) {
        DBusMessageIter structIter;
        dbus_message_iter_open_container(&arrayIter, DBUS_TYPE_STRUCT, nullptr, &structIter);

        int32_t w = width;
        int32_t h = height;
        dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &w);
        dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &h);

        DBusMessageIter byteIter;
        dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "y", &byteIter);
        dbus_message_iter_append_fixed_array(&byteIter, DBUS_TYPE_BYTE, &bytes, static_cast<int>(width * height * 4));
        dbus_message_iter_close_container(&structIter, &byteIter);

        dbus_message_iter_close_container(&arrayIter, &structIter);
    }

    dbus_message_iter_close_container(&varIter, &arrayIter);
    dbus_message_iter_close_container(iter, &varIter);
}

static void AppendDictEntryIconPixmap(DBusMessageIter* dictIter, const char* key, int width, int height, const uint8_t* bytes, size_t byteCount) {
    DBusMessageIter entryIter;
    dbus_message_iter_open_container(dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
    dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &key);
    AppendIconPixmapVariant(&entryIter, width, height, bytes, byteCount);
    dbus_message_iter_close_container(dictIter, &entryIter);
}

void StatusNotifierItemLinux::InitIconThemePath() {
    char exePath[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        std::string dir = exePath;
        size_t lastSlash = dir.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string parentDir = dir.substr(0, lastSlash);
            std::string r1 = parentDir + "/resources";
            if (access((r1 + "/sitstandreminder.png").c_str(), R_OK) == 0) {
                m_iconThemePath = r1;
                m_iconName = r1 + "/sitstandreminder.png";
                return;
            }
            std::string r2 = parentDir + "/../resources";
            if (access((r2 + "/sitstandreminder.png").c_str(), R_OK) == 0) {
                m_iconThemePath = r2;
                m_iconName = r2 + "/sitstandreminder.png";
                return;
            }
        }
    }
    m_iconThemePath = "/usr/share/icons/hicolor/256x256/apps";
    if (access("/usr/share/icons/hicolor/256x256/apps/sitstandreminder.png", R_OK) == 0) {
        m_iconName = "/usr/share/icons/hicolor/256x256/apps/sitstandreminder.png";
    } else {
        m_iconName = "sitstandreminder";
    }
}

DBusMessage* StatusNotifierItemLinux::HandleGetProperty(DBusMessage* msg, const char* iface, const char* prop, StatusNotifierItemLinux* self) {
    (void)iface;
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;

    DBusMessageIter args;
    dbus_message_iter_init_append(reply, &args);

    if (strcmp(prop, "Category") == 0) {
        AppendStringVariant(&args, "ApplicationStatus");
    } else if (strcmp(prop, "Id") == 0) {
        AppendStringVariant(&args, "SitStandReminder");
    } else if (strcmp(prop, "Title") == 0) {
        AppendStringVariant(&args, self->m_title.c_str());
    } else if (strcmp(prop, "Status") == 0) {
        AppendStringVariant(&args, "Active");
    } else if (strcmp(prop, "IconName") == 0) {
        AppendStringVariant(&args, self->m_iconName.c_str());
    } else if (strcmp(prop, "IconThemePath") == 0) {
        if (self->m_iconThemePath.empty()) {
            self->InitIconThemePath();
        }
        AppendStringVariant(&args, self->m_iconThemePath.c_str());
    } else if (strcmp(prop, "IconPixmap") == 0) {
        AppendIconPixmapVariant(&args, self->m_pixmapWidth, self->m_pixmapHeight, self->m_pixmapBytes.data(), self->m_pixmapBytes.size());
    } else if (strcmp(prop, "OverlayIconName") == 0) {
        AppendStringVariant(&args, "");
    } else if (strcmp(prop, "AttentionIconName") == 0) {
        AppendStringVariant(&args, "");
    } else if (strcmp(prop, "ItemIsMenu") == 0) {
        AppendBoolVariant(&args, FALSE);
    } else if (strcmp(prop, "Menu") == 0) {
        const char* menuPath = self->m_menuPath.c_str();
        DBusMessageIter varIter;
        dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "o", &varIter);
        dbus_message_iter_append_basic(&varIter, DBUS_TYPE_OBJECT_PATH, &menuPath);
        dbus_message_iter_close_container(&args, &varIter);
    } else if (strcmp(prop, "ToolTip") == 0) {
        AppendToolTipVariant(&args, "", self->m_tooltipTitle.c_str(), self->m_tooltipDesc.c_str());
    } else if (strcmp(prop, "XAyatanaLabel") == 0) {
        AppendStringVariant(&args, self->m_label.c_str());
    } else if (strcmp(prop, "XAyatanaLabelGuide") == 0) {
        AppendStringVariant(&args, "");
    } else {
        dbus_message_unref(reply);
        return dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
    }

    return reply;
}

DBusMessage* StatusNotifierItemLinux::HandleGetAllProperties(DBusMessage* msg, const char* iface, StatusNotifierItemLinux* self) {
    (void)iface;
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;

    DBusMessageIter args;
    dbus_message_iter_init_append(reply, &args);

    DBusMessageIter dictIter;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dictIter);

    if (self->m_iconThemePath.empty()) {
        self->InitIconThemePath();
    }

    AppendDictEntry(&dictIter, "Category", "ApplicationStatus");
    AppendDictEntry(&dictIter, "Id", "SitStandReminder");
    AppendDictEntry(&dictIter, "Title", self->m_title.c_str());
    AppendDictEntry(&dictIter, "Status", "Active");
    AppendDictEntry(&dictIter, "IconName", self->m_iconName.c_str());
    AppendDictEntry(&dictIter, "IconThemePath", self->m_iconThemePath.c_str());
    AppendDictEntry(&dictIter, "OverlayIconName", "");
    AppendDictEntry(&dictIter, "AttentionIconName", "");
    AppendDictEntryBool(&dictIter, "ItemIsMenu", FALSE);
    AppendDictEntryToolTip(&dictIter, "ToolTip", "", self->m_tooltipTitle.c_str(), self->m_tooltipDesc.c_str());
    AppendDictEntryIconPixmap(&dictIter, "IconPixmap", self->m_pixmapWidth, self->m_pixmapHeight, self->m_pixmapBytes.data(), self->m_pixmapBytes.size());
    AppendDictEntry(&dictIter, "XAyatanaLabel", self->m_label.c_str());
    AppendDictEntry(&dictIter, "XAyatanaLabelGuide", "");

    const char* menuKey = "Menu";
    const char* menuPath = self->m_menuPath.c_str();
    DBusMessageIter entryIter;
    dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
    dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &menuKey);
    DBusMessageIter varIter;
    dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "o", &varIter);
    dbus_message_iter_append_basic(&varIter, DBUS_TYPE_OBJECT_PATH, &menuPath);
    dbus_message_iter_close_container(&entryIter, &varIter);
    dbus_message_iter_close_container(&dictIter, &entryIter);

    dbus_message_iter_close_container(&args, &dictIter);
    return reply;
}

static void AppendMenuItemStruct(DBusMessageIter* parentChildrenIter, int32_t id, const char* label, bool isSeparator) {
    DBusMessageIter varIter;
    dbus_message_iter_open_container(parentChildrenIter, DBUS_TYPE_VARIANT, "(ia{sv}av)", &varIter);

    DBusMessageIter structIter;
    dbus_message_iter_open_container(&varIter, DBUS_TYPE_STRUCT, nullptr, &structIter);

    // 1. id (int32)
    dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &id);

    // 2. properties (a{sv})
    DBusMessageIter dictIter;
    dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);
    if (isSeparator) {
        AppendDictEntry(&dictIter, "type", "separator");
    } else {
        AppendDictEntry(&dictIter, "label", label);
        AppendDictEntryBool(&dictIter, "enabled", TRUE);
    }
    AppendDictEntryBool(&dictIter, "visible", TRUE);
    dbus_message_iter_close_container(&structIter, &dictIter);

    // 3. children (av - empty for leaf item)
    DBusMessageIter subChildrenIter;
    dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "v", &subChildrenIter);
    dbus_message_iter_close_container(&structIter, &subChildrenIter);

    dbus_message_iter_close_container(&varIter, &structIter);
    dbus_message_iter_close_container(parentChildrenIter, &varIter);
}

DBusMessage* StatusNotifierItemLinux::HandleDBusMenuMethod(DBusMessage* msg, StatusNotifierItemLinux* self) {
    const char* member = dbus_message_get_member(msg);
    if (!member) return nullptr;

    if (strcmp(member, "GetLayout") == 0) {
        // 返回 (uint revision, (ia{sv}av) layout)
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (!reply) return nullptr;

        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);
        uint32_t revision = 1;
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &revision);

        // root struct (ia{sv}av)
        DBusMessageIter structIter;
        dbus_message_iter_open_container(&args, DBUS_TYPE_STRUCT, nullptr, &structIter);
        int32_t rootId = 0;
        dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &rootId);

        // a{sv} properties
        DBusMessageIter dictIter;
        dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);
        AppendDictEntry(&dictIter, "children-display", "submenu");
        dbus_message_iter_close_container(&structIter, &dictIter);

        // av children (装填完整托盘菜单项)
        DBusMessageIter childrenIter;
        dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "v", &childrenIter);

        AppendMenuItemStruct(&childrenIter, 2001, "进入工作 (45分钟)", false);
        AppendMenuItemStruct(&childrenIter, 2002, "进入站立 (15分钟)", false);
        AppendMenuItemStruct(&childrenIter, 2003, "立即休息 (2分钟)", false);
        AppendMenuItemStruct(&childrenIter, 10001, "", true); // 分割线
        AppendMenuItemStruct(&childrenIter, 2004, "暂停 / 继续计时", false);
        AppendMenuItemStruct(&childrenIter, 2005, "延后 5 分钟", false);
        AppendMenuItemStruct(&childrenIter, 2006, "跳过当前阶段", false);
        AppendMenuItemStruct(&childrenIter, 10002, "", true); // 分割线
        AppendMenuItemStruct(&childrenIter, 2011, "45m 坐 / 15m 站", false);
        AppendMenuItemStruct(&childrenIter, 2012, "50m 坐 / 10m 站", false);
        AppendMenuItemStruct(&childrenIter, 2013, "25m 番茄工作法", false);
        AppendMenuItemStruct(&childrenIter, 2014, "60m 深度办公", false);
        AppendMenuItemStruct(&childrenIter, 10003, "", true); // 分割线
        AppendMenuItemStruct(&childrenIter, 2020, "设置中心", false);
        AppendMenuItemStruct(&childrenIter, 2021, "退出", false);

        dbus_message_iter_close_container(&structIter, &childrenIter);
        dbus_message_iter_close_container(&args, &structIter);
        return reply;
    } else if (strcmp(member, "GetGroupProperties") == 0) {
        // 输入参数: (ai ids, as propertyNames)
        // 输出参数: (a(ia{sv}) properties)
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (!reply) return nullptr;

        std::vector<int32_t> requestedIds;
        DBusMessageIter args;
        if (dbus_message_iter_init(msg, &args) && dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
            DBusMessageIter subArray;
            dbus_message_iter_recurse(&args, &subArray);
            while (dbus_message_iter_get_arg_type(&subArray) == DBUS_TYPE_INT32) {
                int32_t val = 0;
                dbus_message_iter_get_basic(&subArray, &val);
                requestedIds.push_back(val);
                dbus_message_iter_next(&subArray);
            }
        }

        struct MenuItemMeta {
            int32_t id;
            const char* label;
            bool isSeparator;
        };
        static const MenuItemMeta s_menuDefs[] = {
            { 2001, "进入工作 (45分钟)", false },
            { 2002, "进入站立 (15分钟)", false },
            { 2003, "立即休息 (2分钟)", false },
            { 10001, "", true },
            { 2004, "暂停 / 继续计时", false },
            { 2005, "延后 5 分钟", false },
            { 2006, "跳过当前阶段", false },
            { 10002, "", true },
            { 2011, "45m 坐 / 15m 站", false },
            { 2012, "50m 坐 / 10m 站", false },
            { 2013, "25m 番茄工作法", false },
            { 2014, "60m 深度办公", false },
            { 10003, "", true },
            { 2020, "设置中心", false },
            { 2021, "退出", false }
        };

        DBusMessageIter replyArgs;
        dbus_message_iter_init_append(reply, &replyArgs);

        DBusMessageIter mainArrayIter;
        dbus_message_iter_open_container(&replyArgs, DBUS_TYPE_ARRAY, "(ia{sv})", &mainArrayIter);

        auto appendOneItem = [&](int32_t id, const char* label, bool isSep, bool isRoot) {
            DBusMessageIter structIter;
            dbus_message_iter_open_container(&mainArrayIter, DBUS_TYPE_STRUCT, nullptr, &structIter);
            dbus_message_iter_append_basic(&structIter, DBUS_TYPE_INT32, &id);

            DBusMessageIter dictIter;
            dbus_message_iter_open_container(&structIter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);
            if (isRoot) {
                AppendDictEntry(&dictIter, "children-display", "submenu");
            } else if (isSep) {
                AppendDictEntry(&dictIter, "type", "separator");
                AppendDictEntryBool(&dictIter, "visible", TRUE);
            } else {
                AppendDictEntry(&dictIter, "label", label);
                AppendDictEntryBool(&dictIter, "enabled", TRUE);
                AppendDictEntryBool(&dictIter, "visible", TRUE);
            }
            dbus_message_iter_close_container(&structIter, &dictIter);
            dbus_message_iter_close_container(&mainArrayIter, &structIter);
        };

        if (requestedIds.empty()) {
            appendOneItem(0, "", false, true);
            for (const auto& itm : s_menuDefs) {
                appendOneItem(itm.id, itm.label, itm.isSeparator, false);
            }
        } else {
            for (int32_t qId : requestedIds) {
                if (qId == 0) {
                    appendOneItem(0, "", false, true);
                } else {
                    for (const auto& itm : s_menuDefs) {
                        if (itm.id == qId) {
                            appendOneItem(itm.id, itm.label, itm.isSeparator, false);
                            break;
                        }
                    }
                }
            }
        }

        dbus_message_iter_close_container(&replyArgs, &mainArrayIter);
        return reply;
    } else if (strcmp(member, "GetProperty") == 0) {
        int32_t id = 0;
        const char* propName = nullptr;
        if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_INT32, &id, DBUS_TYPE_STRING, &propName, DBUS_TYPE_INVALID)) {
            DBusMessage* reply = dbus_message_new_method_return(msg);
            if (reply) {
                DBusMessageIter replyArgs;
                dbus_message_iter_init_append(reply, &replyArgs);
                if (id == 0 && propName && strcmp(propName, "children-display") == 0) {
                    AppendStringVariant(&replyArgs, "submenu");
                } else {
                    AppendStringVariant(&replyArgs, "");
                }
                return reply;
            }
        }
    } else if (strcmp(member, "Event") == 0) {
        int32_t id = 0;
        const char* eventId = nullptr;
        DBusMessageIter iter;
        if (dbus_message_iter_init(msg, &iter)) {
            if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                dbus_message_iter_get_basic(&iter, &id);
                dbus_message_iter_next(&iter);
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&iter, &eventId);
                }
            }
        }
        if (eventId && strcmp(eventId, "clicked") == 0) {
            if (self->m_onMenuCommand) {
                self->m_onMenuCommand(static_cast<uint32_t>(id));
            } else if (self->m_onContextMenu) {
                self->m_onContextMenu(0, 0);
            }
        }
        return dbus_message_new_method_return(msg);
    } else if (strcmp(member, "AboutToShow") == 0) {
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_bool_t needUpdate = FALSE;
            dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &needUpdate, DBUS_TYPE_INVALID);
        }
        return reply;
    }
    return dbus_message_new_method_return(msg);
}

int StatusNotifierItemLinux::MessageFilter(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    auto* self = static_cast<StatusNotifierItemLinux*>(user_data);
    const char* path = dbus_message_get_path(msg);
    const char* iface = dbus_message_get_interface(msg);
    const char* member = dbus_message_get_member(msg);

    // 捕获 Watcher 重新注册信号 (处理 GNOME Shell 崩溃重载或重启)
    if (dbus_message_is_signal(msg, "org.kde.StatusNotifierWatcher", "StatusNotifierWatcherRegistered")) {
        std::cout << "[SNI] Watcher registered signal received, re-registering..." << std::endl;
        self->RegisterWithWatcher();
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!path || !member) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // 响应标准 Introspect 接口
    if (iface && strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 && strcmp(member, "Introspect") == 0) {
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            const char* xmlData = "";
            if (strcmp(path, "/StatusNotifierItem") == 0) {
                xmlData = 
                    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
                    "<node>\n"
                    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                    "    <method name=\"Introspect\">\n"
                    "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "  </interface>\n"
                    "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
                    "    <method name=\"Get\">\n"
                    "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
                    "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
                    "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "    <method name=\"GetAll\">\n"
                    "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
                    "      <arg name=\"props\" type=\"a{sv}\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "  </interface>\n"
                    "  <interface name=\"org.kde.StatusNotifierItem\">\n"
                    "    <property name=\"Category\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"Id\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"Title\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"Status\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"IconName\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"IconThemePath\" type=\"s\" access=\"read\"/>\n"
                    "    <property name=\"Menu\" type=\"o\" access=\"read\"/>\n"
                    "    <property name=\"ItemIsMenu\" type=\"b\" access=\"read\"/>\n"
                    "    <property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>\n"
                    "    <property name=\"ToolTip\" type=\"(sa(iiay)ss)\" access=\"read\"/>\n"
                    "    <property name=\"XAyatanaLabel\" type=\"s\" access=\"read\"/>\n"
                    "    <method name=\"ContextMenu\">\n"
                    "      <arg name=\"x\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"y\" type=\"i\" direction=\"in\"/>\n"
                    "    </method>\n"
                    "    <method name=\"Activate\">\n"
                    "      <arg name=\"x\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"y\" type=\"i\" direction=\"in\"/>\n"
                    "    </method>\n"
                    "    <signal name=\"NewIcon\"/>\n"
                    "    <signal name=\"NewToolTip\"/>\n"
                    "  </interface>\n"
                    "</node>\n";
            } else if (strcmp(path, "/MenuBar") == 0) {
                xmlData = 
                    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
                    "<node>\n"
                    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                    "    <method name=\"Introspect\">\n"
                    "      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "  </interface>\n"
                    "  <interface name=\"com.canonical.dbusmenu\">\n"
                    "    <method name=\"GetLayout\">\n"
                    "      <arg name=\"parentId\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"recursionDepth\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"propertyNames\" type=\"as\" direction=\"in\"/>\n"
                    "      <arg name=\"revision\" type=\"u\" direction=\"out\"/>\n"
                    "      <arg name=\"layout\" type=\"(ia{sv}av)\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "    <method name=\"GetGroupProperties\">\n"
                    "      <arg name=\"ids\" type=\"ai\" direction=\"in\"/>\n"
                    "      <arg name=\"propertyNames\" type=\"as\" direction=\"in\"/>\n"
                    "      <arg name=\"properties\" type=\"a(ia{sv})\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "    <method name=\"GetProperty\">\n"
                    "      <arg name=\"id\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"name\" type=\"s\" direction=\"in\"/>\n"
                    "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "    <method name=\"AboutToShow\">\n"
                    "      <arg name=\"id\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"needUpdate\" type=\"b\" direction=\"out\"/>\n"
                    "    </method>\n"
                    "    <method name=\"Event\">\n"
                    "      <arg name=\"id\" type=\"i\" direction=\"in\"/>\n"
                    "      <arg name=\"eventId\" type=\"s\" direction=\"in\"/>\n"
                    "      <arg name=\"data\" type=\"v\" direction=\"in\"/>\n"
                    "      <arg name=\"timestamp\" type=\"u\" direction=\"in\"/>\n"
                    "    </method>\n"
                    "  </interface>\n"
                    "</node>\n";
            }
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &xmlData, DBUS_TYPE_INVALID);
            if (conn) {
                dbus_connection_send(conn, reply, nullptr);
            }
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    if (strcmp(path, "/StatusNotifierItem") == 0) {
        if (iface && strcmp(iface, "org.freedesktop.DBus.Properties") == 0) {
            if (strcmp(member, "Get") == 0) {
                const char* queryIface = nullptr;
                const char* propName = nullptr;
                if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_STRING, &queryIface, DBUS_TYPE_STRING, &propName, DBUS_TYPE_INVALID)) {
                    DBusMessage* reply = HandleGetProperty(msg, queryIface, propName, self);
                    if (reply) {
                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                        return DBUS_HANDLER_RESULT_HANDLED;
                    }
                }
            } else if (strcmp(member, "GetAll") == 0) {
                const char* queryIface = nullptr;
                if (dbus_message_get_args(msg, nullptr, DBUS_TYPE_STRING, &queryIface, DBUS_TYPE_INVALID)) {
                    DBusMessage* reply = HandleGetAllProperties(msg, queryIface, self);
                    if (reply) {
                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                        return DBUS_HANDLER_RESULT_HANDLED;
                    }
                }
            }
        } else if (iface && (strcmp(iface, "org.kde.StatusNotifierItem") == 0 ||
                             strcmp(iface, "org.freedesktop.StatusNotifierItem") == 0)) {
            if (strcmp(member, "ContextMenu") == 0) {
                int x = 0, y = 0;
                dbus_message_get_args(msg, nullptr, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y, DBUS_TYPE_INVALID);
                if (self->m_onContextMenu) {
                    self->m_onContextMenu(x, y);
                }
                DBusMessage* reply = dbus_message_new_method_return(msg);
                if (reply) {
                    if (conn) dbus_connection_send(conn, reply, nullptr);
                    dbus_message_unref(reply);
                }
                return DBUS_HANDLER_RESULT_HANDLED;
            } else if (strcmp(member, "Activate") == 0) {
                int x = 0, y = 0;
                dbus_message_get_args(msg, nullptr, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y, DBUS_TYPE_INVALID);
                if (self->m_onActivate) {
                    self->m_onActivate(x, y);
                }
                DBusMessage* reply = dbus_message_new_method_return(msg);
                if (reply) {
                    if (conn) dbus_connection_send(conn, reply, nullptr);
                    dbus_message_unref(reply);
                }
                return DBUS_HANDLER_RESULT_HANDLED;
            } else if (strcmp(member, "SecondaryActivate") == 0 || strcmp(member, "Scroll") == 0) {
                DBusMessage* reply = dbus_message_new_method_return(msg);
                if (reply) {
                    if (conn) dbus_connection_send(conn, reply, nullptr);
                    dbus_message_unref(reply);
                }
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }
    } else if (strcmp(path, "/MenuBar") == 0) {
        // 处理 com.canonical.dbusmenu 接口调用
        DBusMessage* reply = HandleDBusMenuMethod(msg, self);
        if (reply) {
            if (conn) dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool StatusNotifierItemLinux::RegisterWithWatcher() {
    if (!m_conn) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        "org.kde.StatusNotifierWatcher",
        "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher",
        "RegisterStatusNotifierItem"
    );
    if (!msg) return false;

    const char* svc = m_serviceName.c_str();
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &svc, DBUS_TYPE_INVALID);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 2000, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        std::cerr << "[SNI] Failed to register with StatusNotifierWatcher: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    if (reply) {
        dbus_message_unref(reply);
    }
    return true;
}

static std::atomic<int> s_sniCounter{1};

bool StatusNotifierItemLinux::Initialize(
    std::function<void(int x, int y)> onContextMenu,
    std::function<void(int x, int y)> onActivate
) {
    if (m_conn) return true;

    InitIconThemePath();

    m_onContextMenu = std::move(onContextMenu);
    m_onActivate = std::move(onActivate);

    DBusError err;
    dbus_error_init(&err);
    m_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (!m_conn || dbus_error_is_set(&err)) {
        std::cerr << "[SNI] Failed to connect to Session D-Bus: " << (err.message ? err.message : "unknown") << std::endl;
        dbus_error_free(&err);
        m_conn = nullptr;
        return false;
    }

    // 申请专用服务名称 org.kde.StatusNotifierItem-{PID}-{counter}
    m_serviceName = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-" + std::to_string(s_sniCounter.fetch_add(1));
    int reqResult = dbus_bus_request_name(m_conn, m_serviceName.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err) || (reqResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && reqResult != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)) {
        std::cerr << "[SNI] Failed to request D-Bus name: " << m_serviceName << std::endl;
        dbus_error_free(&err);
        dbus_connection_close(m_conn);
        dbus_connection_unref(m_conn);
        m_conn = nullptr;
        return false;
    }

    // 监听 Watcher 注册信号
    dbus_bus_add_match(m_conn, "type='signal',interface='org.kde.StatusNotifierWatcher',member='StatusNotifierWatcherRegistered'", nullptr);

    // 安装消息过滤器
    dbus_connection_add_filter(m_conn, reinterpret_cast<DBusHandleMessageFunction>(MessageFilter), this, nullptr);

    // 向 Watcher 注册
    m_registered = RegisterWithWatcher();
    if (m_registered) {
        std::cout << "[SNI] StatusNotifierItem successfully registered: " << m_serviceName << std::endl;
    }
    return m_registered;
}

void StatusNotifierItemLinux::Shutdown() {
    if (m_conn) {
        dbus_connection_remove_filter(m_conn, reinterpret_cast<DBusHandleMessageFunction>(MessageFilter), this);
        dbus_connection_close(m_conn);
        dbus_connection_unref(m_conn);
        m_conn = nullptr;
    }
    m_registered = false;
}

void StatusNotifierItemLinux::ProcessEvents() {
    if (!m_conn) return;
    dbus_connection_read_write(m_conn, 0);
    while (dbus_connection_get_dispatch_status(m_conn) == DBUS_DISPATCH_DATA_REMAINS) {
        dbus_connection_dispatch(m_conn);
    }
}

void StatusNotifierItemLinux::SetTooltip(const std::string& title, const std::string& description) {
    m_tooltipTitle = title;
    m_tooltipDesc = description;
    if (m_conn) {
        DBusMessage* sig = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip");
        if (sig) {
            dbus_connection_send(m_conn, sig, nullptr);
            dbus_message_unref(sig);
        }
    }
}

void StatusNotifierItemLinux::SetIconName(const std::string& iconName) {
    m_iconName = iconName;
    if (m_conn) {
        DBusMessage* sig = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon");
        if (sig) {
            dbus_connection_send(m_conn, sig, nullptr);
            dbus_message_unref(sig);
        }
    }
}

void StatusNotifierItemLinux::SetIconPixmap(int width, int height, const uint32_t* argbData) {
    if (width <= 0 || height <= 0 || !argbData) return;

    m_pixmapWidth = width;
    m_pixmapHeight = height;
    m_pixmapBytes.resize(width * height * 4);

    // SNI 规范要求 ARGB 网络字节序 (Big-Endian)
    for (int i = 0; i < width * height; ++i) {
        uint32_t pixel = argbData[i];
        m_pixmapBytes[i * 4 + 0] = static_cast<uint8_t>((pixel >> 24) & 0xFF); // A
        m_pixmapBytes[i * 4 + 1] = static_cast<uint8_t>((pixel >> 16) & 0xFF); // R
        m_pixmapBytes[i * 4 + 2] = static_cast<uint8_t>((pixel >> 8) & 0xFF);  // G
        m_pixmapBytes[i * 4 + 3] = static_cast<uint8_t>(pixel & 0xFF);         // B
    }

    // 针对 GNOME Shell (ubuntu-appindicators) 与 KDE/XFCE 动态托盘机制：
    // 根据 ubuntu-appindicators 官方实现 (appIndicator.js 第 797-810 行)，
    // 若 IconName 非空，GNOME 优先使用 IconName 并将其强行加入带 10 秒续期的静态 IconCache 中，
    // 导致无论底层文件内容如何改变均拒绝重新读取。
    // 而当 IconName 为空字符串 ("") 时，GNOME 100% 走 _createIconFromPixmap 分支，
    // 由 GdkPixbuf.Pixbuf.new_from_bytes 直接由内存流生成全新 Pixbuf 呈现，完全避开静态缓存。
    m_iconName.clear();

    if (m_conn) {
        DBusMessage* sig = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon");
        if (sig) {
            dbus_connection_send(m_conn, sig, nullptr);
            dbus_message_unref(sig);
        }
        dbus_connection_flush(m_conn);
    }
}

void StatusNotifierItemLinux::SetLabel(const std::string& label) {
    m_label = label;
    if (m_conn) {
        DBusMessage* sig = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "XAyatanaNewLabel");
        if (sig) {
            dbus_connection_send(m_conn, sig, nullptr);
            dbus_message_unref(sig);
        }
    }
}

int StatusNotifierItemLinux::GetConnectionSocket() const {
    if (!m_conn) return -1;
    int fd = -1;
    if (dbus_connection_get_socket(m_conn, &fd) && fd >= 0) {
        return fd;
    }
    return -1;
}

#endif
