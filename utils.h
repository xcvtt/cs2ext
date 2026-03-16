#pragma once
#include <chrono>
#include <thread>

#include <windows.h>
#pragma comment(lib, "winmm.lib")

inline void limit_frame(std::chrono::high_resolution_clock::time_point frame_start,
                        double target_fps) {
    using namespace std::chrono;

    static bool timer_initialized = false;
    if (!timer_initialized) {
        timeBeginPeriod(1);
        timer_initialized = true;
    }

    long long target_ns = static_cast<long long>(1'000'000'000.0 / target_fps);
    nanoseconds target(target_ns);

    nanoseconds elapsed = high_resolution_clock::now() - frame_start;
    nanoseconds remaining = target - elapsed;

    if (remaining > milliseconds(2)) {
        nanoseconds sleep_time = remaining - microseconds(1500);
        std::this_thread::sleep_for(sleep_time);
    }

    while (high_resolution_clock::now() - frame_start < target) {
        _mm_pause();
    }
}

inline ImU32 apply_opacity(ImU32 col, float opacity) {
    if (opacity >= 1.0f) return col;
    ImU32 a = (ImU32)(((col >> 24) & 0xFF) * opacity);
    return (col & 0x00FFFFFF) | (a << 24);
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
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_ESCAPE: return "Escape";
    default: {
        static char buf[32];
        UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
        if (GetKeyNameTextA(scan << 16, buf, 32)) return buf;
        snprintf(buf, 32, "Key 0x%02X", vk);
        return buf;
    }
    }
}

inline int scan_any_key() {
    for (int k = VK_F1; k <= VK_F12; k++)
        if (GetAsyncKeyState(k) & 1) return k;
    for (int k : {VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT})
        if (GetAsyncKeyState(k) & 1) return k;
    for (int k : {VK_XBUTTON1, VK_XBUTTON2, VK_MBUTTON})
        if (GetAsyncKeyState(k) & 1) return k;
    if (GetAsyncKeyState(VK_ESCAPE) & 1) return -1;
    return 0;
}