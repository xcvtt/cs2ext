#pragma once
#include <chrono>
#include <thread>
#include <Windows.h>

inline void limit_frame(std::chrono::high_resolution_clock::time_point frame_start,
                        double target_fps = 60.0) {
    using namespace std::chrono;
    nanoseconds target(static_cast<long long>(1'000'000'000.0 / target_fps));
    nanoseconds remaining = target - (high_resolution_clock::now() - frame_start);
    if (remaining > nanoseconds::zero()) {
        nanoseconds sleep_time = remaining - milliseconds(2);
        if (sleep_time > nanoseconds::zero())
            std::this_thread::sleep_for(sleep_time);
        while (high_resolution_clock::now() - frame_start < target) {
        }
    }
}

inline const char* vk_name(int vk) {
    switch (vk) {
        case VK_LBUTTON: return "Mouse1";
        case VK_RBUTTON: return "Mouse2";
        case VK_MBUTTON: return "Mouse3";
        case VK_XBUTTON1: return "Mouse4";
        case VK_XBUTTON2: return "Mouse5";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        case VK_CAPITAL: return "CapsLock";
        case VK_TAB: return "Tab";
        case VK_SPACE: return "Space";
        default: {
            static char buf[32];
            UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextA(scan << 16, buf, 32)) return buf;
            snprintf(buf, 32, "Key 0x%02X", vk);
            return buf;
        }
    }
}