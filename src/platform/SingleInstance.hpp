#pragma once

#include <functional>
#include <string>

class SingleInstance {
public:
    static SingleInstance& Instance() {
        static SingleInstance inst;
        return inst;
    }

    bool TryAcquire();
    void Release();
    void WakeExistingInstance();

    using WakeCallback = std::function<void()>;
    void SetOnWakeRequested(WakeCallback cb) { m_onWake = std::move(cb); }
    void TriggerWake() { if (m_onWake) m_onWake(); }
    bool CheckAndResetWakeRequested();

private:
    SingleInstance() = default;
    ~SingleInstance() { Release(); }

#ifdef _WIN32
    void* m_hMutex = nullptr;
#else
    int m_lockFd = -1;
    std::string m_lockPath;
#endif
    WakeCallback m_onWake;
};
