#pragma once
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "settings.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct FontEntry {
    std::string display_name;
    std::string path;
};

class Overlay {
public:
    HWND overlay_hwnd = nullptr;
    HWND game_hwnd    = nullptr;
    int  width = 0, height = 0;

    ImFont* default_font     = nullptr;
    ImFont* esp_font        = nullptr;
    ImFont* esp_font_name   = nullptr;
    ImFont* esp_font_hp     = nullptr;
    ImFont* esp_font_weapon = nullptr;
    ImFont* esp_font_nade   = nullptr;
    ImFont* spec_font        = nullptr;
    ImFont* menu_font        = nullptr;
    ImFont* menu_title_font  = nullptr;

    std::vector<FontEntry> available_fonts;
    std::vector<FontEntry> menu_fonts;
    bool font_rebuild_needed = false;

    ID3D11Device* get_device() const { return device; }

    // -------------------------------------------------------------------------
    bool init(const wchar_t* target_window) {
        if (!resolver_.resolve_win32k()) {
            printf("[-] win32k SSN resolution failed\n");
            return false;
        }

        if (!invoker_.init_win32k()) {
            printf("[-] win32k stub allocation failed\n");
            return false;
        }

        // Wire up Win32k syscalls
        win32k_.init(invoker_, resolver_);

        game_hwnd = win32k_.find_window(nullptr, target_window);
        if (!game_hwnd) return false;

        RECT rc;
        GetClientRect(game_hwnd, &rc);
        width  = rc.right;
        height = rc.bottom;

        // Find Discord's overlay — fully through syscall
        overlay_hwnd = win32k_.find_window(L"Chrome_WidgetWin_1", L"Discord Overlay"
        );
        if (overlay_hwnd) {
            printf("[+] Using discord overlay\n");
            using_discord_overlay = true;
        }
        else {
            printf("[+] Discord overlay not found. Falling back to CreateWindowExW\n");
            wchar_t class_name[16];
            srand(static_cast<unsigned>(GetTickCount64()));
            for (int i = 0; i < 12; i++)
                class_name[i] = L'a' + (rand() % 26);
            class_name[12] = 0;

            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof(wc);
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = wnd_proc;
            wc.hInstance     = GetModuleHandle(nullptr);
            wc.lpszClassName = class_name;
            RegisterClassExW(&wc);

            overlay_hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                wc.lpszClassName, L"", WS_POPUP,
                0, 0, width, height,
                nullptr, nullptr, wc.hInstance, nullptr);

            if (!overlay_hwnd) return false;

            SetLayeredWindowAttributes(overlay_hwnd, 0, 255, LWA_ALPHA);
            MARGINS margins = {-1};
            DwmExtendFrameIntoClientArea(overlay_hwnd, &margins);

            RECT game_rect;
            GetWindowRect(game_hwnd, &game_rect);
            SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                         game_rect.left, game_rect.top, width, height, SWP_SHOWWINDOW);
        }

        if (!init_dx11()) return false;

        scan_fonts();
        init_imgui();

        return true;
    }

    bool is_game_window() const {
        if (!game_hwnd || !IsWindow(game_hwnd))
            return false;

        wchar_t title[128]{};
        GetWindowTextW(game_hwnd, title, 128);

        if (wcscmp(title, L"Counter-Strike 2") != 0)
            return false;

        if (!IsWindowVisible(game_hwnd) || IsIconic(game_hwnd))
            return false;

        BOOL cloaked = FALSE;
        if (SUCCEEDED(DwmGetWindowAttribute(
                game_hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
            if (cloaked)
                return false;
                }

        HWND fg = GetForegroundWindow();
        if (fg != game_hwnd && fg != overlay_hwnd)
            return false;

        return true;
    }

    // -------------------------------------------------------------------------
    // Called when the user opens/closes the in-game menu.
    // interactive=true  → steals focus + cursor so ImGui widgets are clickable.
    // interactive=false → returns focus + cursor to the game.
    // -------------------------------------------------------------------------
    void set_interactive(bool interactive) {
        LONG_PTR ex = GetWindowLongW(overlay_hwnd, GWL_EXSTYLE);

        if (interactive) {
            // Remove click-through
            if (ex & WS_EX_TRANSPARENT) {
                win32k_.set_window_long(
                    overlay_hwnd, GWL_EXSTYLE,
                    ex & ~WS_EX_TRANSPARENT
                );
            }

            win32k_.set_window_pos(
                overlay_hwnd, HWND_TOPMOST,
                0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
            );

            force_foreground(overlay_hwnd);
        } else {
            // Restore click-through
            if (!(ex & WS_EX_TRANSPARENT)) {
                win32k_.set_window_long(
                    overlay_hwnd, GWL_EXSTYLE,
                    ex | WS_EX_TRANSPARENT
                );
            }

            win32k_.set_window_pos(
                overlay_hwnd, HWND_TOPMOST,
                0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
            );

            if (game_hwnd && IsWindow(game_hwnd)) {
                force_foreground(game_hwnd);
            }
        }
    }

    // -------------------------------------------------------------------------
    void rebuild_fonts() {
        auto& io = ImGui::GetIO();
        ImGui_ImplDX11_InvalidateDeviceObjects();
        io.Fonts->Clear();

        const char* mf_path = get_menu_font_path();
        float       mf_size = g_settings.menu_font_size;

        auto make_cfg = [](int oversample_h = 2, int oversample_v = 1) {
            ImFontConfig cfg;
            cfg.OversampleH = oversample_h;
            cfg.OversampleV = oversample_v;
            cfg.PixelSnapH  = true;
            return cfg;
        };

        // ---- Menu fonts ----
        if (mf_path) {
            auto cfg = make_cfg();
            default_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size, &cfg, io.Fonts->GetGlyphRangesDefault());

            cfg = make_cfg();
            menu_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size, &cfg, io.Fonts->GetGlyphRangesDefault());

            cfg = make_cfg();
            menu_title_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size, &cfg, io.Fonts->GetGlyphRangesDefault());
        }
        if (!default_font)    default_font    = io.Fonts->AddFontDefault();
        if (!menu_font)       menu_font       = default_font;
        if (!menu_title_font) menu_title_font = default_font;

        // ---- Spectator font ----
        if (mf_path) {
            auto cfg = make_cfg();
            spec_font = io.Fonts->AddFontFromFileTTF(
                mf_path, 13.0f, &cfg, get_glyph_ranges());
        }
        if (!spec_font) spec_font = io.Fonts->AddFontDefault();

        // ---- ESP fonts ----
        const char* esp_path = get_esp_font_path();
        const char* esp_fb   = find_system_font("tahoma.ttf");

        auto build_esp_font = [&](float size) -> ImFont* {
            size = std::roundf(std::max(size, 8.0f));  // round to integer px
            auto c = make_cfg();
            ImFont* f = nullptr;
            if (esp_path)
                f = io.Fonts->AddFontFromFileTTF(esp_path, size, &c, get_glyph_ranges());
            if (!f && esp_fb)
                f = io.Fonts->AddFontFromFileTTF(esp_fb, size, &c, get_glyph_ranges());
            return f;
        };

        esp_font_name   = build_esp_font(g_settings.name_font_size);
        esp_font_hp     = build_esp_font(g_settings.hp_font_size);
        esp_font_weapon = build_esp_font(g_settings.weapon_font_size);
        esp_font_nade   = build_esp_font(g_settings.grenade_text_font_size);

        esp_font = esp_font_name;
        if (!esp_font)        esp_font        = io.Fonts->AddFontDefault();
        if (!esp_font_hp)     esp_font_hp     = esp_font;
        if (!esp_font_weapon) esp_font_weapon = esp_font;
        if (!esp_font_nade)   esp_font_nade   = esp_font;

        g_settings.esp_font_atlas_size = std::max(g_settings.name_font_size, 8.0f);

        io.Fonts->Build();

        ImGui_ImplDX11_CreateDeviceObjects();
        font_rebuild_needed = false;
    }

    // -------------------------------------------------------------------------
    bool begin_frame() {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(game_hwnd)) return false;
        if (font_rebuild_needed) rebuild_fonts();

        static int check_tick = 0;
        if (++check_tick >= 10) {
            check_tick = 0;
            update_window_tracking();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if (using_discord_overlay && g_settings.menu_open) {
            update_input();
        }

        ImGui::NewFrame();
        return true;
    }

    // -------------------------------------------------------------------------
    void end_frame(int sync_interval) {
        ImGui::Render();
        const float clear[4] = {0, 0, 0, 0};
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap_chain->Present(sync_interval, 0);
    }

    void shutdown() {
        if (using_discord_overlay && swap_chain && context && rtv) {
            float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            context->ClearRenderTargetView(rtv, clear_color);
            swap_chain->Present(1, 0);
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (rtv)        { rtv->Release();        rtv = nullptr; }
        if (swap_chain) { swap_chain->Release();  swap_chain = nullptr; }
        if (context)    { context->Release();     context = nullptr; }
        if (device)     { device->Release();      device = nullptr; }

        if (!using_discord_overlay)
            win32k_.destroy_window(overlay_hwnd);
    }

    // -------------------------------------------------------------------------
    void apply_menu_style() {
        auto& s = ImGui::GetStyle();
        ImVec4* colors = s.Colors;

        ImVec4 accent = { g_settings.menu_accent_color[0], g_settings.menu_accent_color[1],
                          g_settings.menu_accent_color[2], g_settings.menu_accent_color[3] };
        ImVec4 accent_dim = { accent.x * 0.6f, accent.y * 0.6f,
                              accent.z * 0.6f, accent.w * 0.7f };
        ImVec4 border = { g_settings.menu_border_color[0], g_settings.menu_border_color[1],
                          g_settings.menu_border_color[2], g_settings.menu_border_color[3] };
        float bg_a = g_settings.menu_bg_alpha;

        colors[ImGuiCol_WindowBg]             = { 0.06f, 0.06f, 0.08f, bg_a };
        colors[ImGuiCol_ChildBg]              = { 0.07f, 0.07f, 0.09f, bg_a * 0.5f };
        colors[ImGuiCol_PopupBg]              = { 0.08f, 0.08f, 0.10f, bg_a };
        colors[ImGuiCol_Border]               = border;
        colors[ImGuiCol_BorderShadow]         = { 0, 0, 0, 0 };
        colors[ImGuiCol_FrameBg]              = { 0.10f, 0.10f, 0.12f, 0.8f };
        colors[ImGuiCol_FrameBgHovered]       = { accent.x*0.2f, accent.y*0.2f, accent.z*0.2f, 0.6f };
        colors[ImGuiCol_FrameBgActive]        = { accent.x*0.3f, accent.y*0.3f, accent.z*0.3f, 0.8f };
        colors[ImGuiCol_TitleBg]              = { 0.04f, 0.04f, 0.06f, bg_a };
        colors[ImGuiCol_TitleBgActive]        = { accent.x*0.1f, accent.y*0.1f, accent.z*0.1f, bg_a };
        colors[ImGuiCol_TitleBgCollapsed]     = { 0.04f, 0.04f, 0.06f, 0.5f };
        colors[ImGuiCol_Tab]                  = { 0.08f, 0.08f, 0.10f, 0.8f };
        colors[ImGuiCol_TabHovered]           = { accent.x*0.4f, accent.y*0.4f, accent.z*0.4f, 0.8f };
        colors[ImGuiCol_TabActive]            = { accent.x*0.2f, accent.y*0.2f, accent.z*0.2f, 1.0f };
        colors[ImGuiCol_TabUnfocused]         = { 0.06f, 0.06f, 0.08f, 0.8f };
        colors[ImGuiCol_TabUnfocusedActive]   = { accent.x*0.15f, accent.y*0.15f, accent.z*0.15f, 0.9f };
        colors[ImGuiCol_Button]               = { 0.12f, 0.12f, 0.14f, 0.8f };
        colors[ImGuiCol_ButtonHovered]        = { accent.x*0.3f, accent.y*0.3f, accent.z*0.3f, 0.8f };
        colors[ImGuiCol_ButtonActive]         = { accent.x*0.5f, accent.y*0.5f, accent.z*0.5f, 1.0f };
        colors[ImGuiCol_Header]               = { accent.x*0.15f, accent.y*0.15f, accent.z*0.15f, 0.6f };
        colors[ImGuiCol_HeaderHovered]        = { accent.x*0.25f, accent.y*0.25f, accent.z*0.25f, 0.8f };
        colors[ImGuiCol_HeaderActive]         = { accent.x*0.3f, accent.y*0.3f, accent.z*0.3f, 1.0f };
        colors[ImGuiCol_SliderGrab]           = accent_dim;
        colors[ImGuiCol_SliderGrabActive]     = accent;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_Separator]            = { accent.x*0.3f, accent.y*0.3f, accent.z*0.3f, 0.5f };
        colors[ImGuiCol_SeparatorHovered]     = accent_dim;
        colors[ImGuiCol_SeparatorActive]      = accent;
        colors[ImGuiCol_ScrollbarBg]          = { 0.05f, 0.05f, 0.07f, 0.5f };
        colors[ImGuiCol_ScrollbarGrab]        = { 0.15f, 0.15f, 0.18f, 0.8f };
        colors[ImGuiCol_ScrollbarGrabHovered] = accent_dim;
        colors[ImGuiCol_ScrollbarGrabActive]  = accent;
        colors[ImGuiCol_Text]                 = { 0.85f, 0.90f, 0.88f, 1.0f };
        colors[ImGuiCol_TextDisabled]         = { 0.40f, 0.45f, 0.43f, 1.0f };
        colors[ImGuiCol_ResizeGrip]           = { accent.x*0.2f, accent.y*0.2f, accent.z*0.2f, 0.3f };
        colors[ImGuiCol_ResizeGripHovered]    = accent_dim;
        colors[ImGuiCol_ResizeGripActive]     = accent;

        s.WindowRounding   = 4.0f;
        s.FrameRounding    = 3.0f;
        s.GrabRounding     = 2.0f;
        s.TabRounding      = 3.0f;
        s.ScrollbarRounding= 2.0f;
        s.WindowBorderSize = 1.0f;
        s.FrameBorderSize  = 0.0f;
        s.PopupBorderSize  = 1.0f;
        s.WindowPadding    = { 10, 10 };
        s.FramePadding     = { 6,  4  };
        s.ItemSpacing      = { 8,  6  };
        s.ItemInnerSpacing = { 6,  4  };
        s.Alpha            = 1.0f;
    }

// =============================================================================
private:
// =============================================================================
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    bool was_visible = true;
    RECT last_game_rect{};

    bool using_discord_overlay = false;

    SyscallInvoker  invoker_;
    SyscallResolver resolver_;
    Win32kSyscall   win32k_;

    static ImGuiKey VirtualKeyToImGuiKey(int vk)
    {
        switch (vk)
        {
            case VK_TAB:        return ImGuiKey_Tab;
            case VK_LEFT:       return ImGuiKey_LeftArrow;
            case VK_RIGHT:      return ImGuiKey_RightArrow;
            case VK_UP:         return ImGuiKey_UpArrow;
            case VK_DOWN:       return ImGuiKey_DownArrow;
            case VK_PRIOR:      return ImGuiKey_PageUp;
            case VK_NEXT:       return ImGuiKey_PageDown;
            case VK_HOME:       return ImGuiKey_Home;
            case VK_END:        return ImGuiKey_End;
            case VK_INSERT:     return ImGuiKey_Insert;
            case VK_DELETE:     return ImGuiKey_Delete;
            case VK_BACK:       return ImGuiKey_Backspace;
            case VK_SPACE:      return ImGuiKey_Space;
            case VK_RETURN:     return ImGuiKey_Enter;
            case VK_ESCAPE:     return ImGuiKey_Escape;
            case VK_OEM_7:      return ImGuiKey_Apostrophe;
            case VK_OEM_COMMA:  return ImGuiKey_Comma;
            case VK_OEM_MINUS:  return ImGuiKey_Minus;
            case VK_OEM_PERIOD: return ImGuiKey_Period;
            case VK_OEM_2:      return ImGuiKey_Slash;
            case VK_OEM_1:      return ImGuiKey_Semicolon;
            case VK_OEM_PLUS:   return ImGuiKey_Equal;
            case VK_OEM_4:      return ImGuiKey_LeftBracket;
            case VK_OEM_5:      return ImGuiKey_Backslash;
            case VK_OEM_6:      return ImGuiKey_RightBracket;
            case VK_OEM_3:      return ImGuiKey_GraveAccent;
            case VK_CAPITAL:    return ImGuiKey_CapsLock;
            case VK_SCROLL:     return ImGuiKey_ScrollLock;
            case VK_NUMLOCK:    return ImGuiKey_NumLock;
            case VK_SNAPSHOT:   return ImGuiKey_PrintScreen;
            case VK_PAUSE:      return ImGuiKey_Pause;
            case VK_NUMPAD0:    return ImGuiKey_Keypad0;
            case VK_NUMPAD1:    return ImGuiKey_Keypad1;
            case VK_NUMPAD2:    return ImGuiKey_Keypad2;
            case VK_NUMPAD3:    return ImGuiKey_Keypad3;
            case VK_NUMPAD4:    return ImGuiKey_Keypad4;
            case VK_NUMPAD5:    return ImGuiKey_Keypad5;
            case VK_NUMPAD6:    return ImGuiKey_Keypad6;
            case VK_NUMPAD7:    return ImGuiKey_Keypad7;
            case VK_NUMPAD8:    return ImGuiKey_Keypad8;
            case VK_NUMPAD9:    return ImGuiKey_Keypad9;
            case VK_DECIMAL:    return ImGuiKey_KeypadDecimal;
            case VK_DIVIDE:     return ImGuiKey_KeypadDivide;
            case VK_MULTIPLY:   return ImGuiKey_KeypadMultiply;
            case VK_SUBTRACT:   return ImGuiKey_KeypadSubtract;
            case VK_ADD:        return ImGuiKey_KeypadAdd;
            case VK_LSHIFT:     return ImGuiKey_LeftShift;
            case VK_LCONTROL:   return ImGuiKey_LeftCtrl;
            case VK_LMENU:      return ImGuiKey_LeftAlt;
            case VK_LWIN:       return ImGuiKey_LeftSuper;
            case VK_RSHIFT:     return ImGuiKey_RightShift;
            case VK_RCONTROL:   return ImGuiKey_RightCtrl;
            case VK_RMENU:      return ImGuiKey_RightAlt;
            case VK_RWIN:       return ImGuiKey_RightSuper;
            case VK_APPS:       return ImGuiKey_Menu;
            case '0':           return ImGuiKey_0;
            case '1':           return ImGuiKey_1;
            case '2':           return ImGuiKey_2;
            case '3':           return ImGuiKey_3;
            case '4':           return ImGuiKey_4;
            case '5':           return ImGuiKey_5;
            case '6':           return ImGuiKey_6;
            case '7':           return ImGuiKey_7;
            case '8':           return ImGuiKey_8;
            case '9':           return ImGuiKey_9;
            case 'A':           return ImGuiKey_A;
            case 'B':           return ImGuiKey_B;
            case 'C':           return ImGuiKey_C;
            case 'D':           return ImGuiKey_D;
            case 'E':           return ImGuiKey_E;
            case 'F':           return ImGuiKey_F;
            case 'G':           return ImGuiKey_G;
            case 'H':           return ImGuiKey_H;
            case 'I':           return ImGuiKey_I;
            case 'J':           return ImGuiKey_J;
            case 'K':           return ImGuiKey_K;
            case 'L':           return ImGuiKey_L;
            case 'M':           return ImGuiKey_M;
            case 'N':           return ImGuiKey_N;
            case 'O':           return ImGuiKey_O;
            case 'P':           return ImGuiKey_P;
            case 'Q':           return ImGuiKey_Q;
            case 'R':           return ImGuiKey_R;
            case 'S':           return ImGuiKey_S;
            case 'T':           return ImGuiKey_T;
            case 'U':           return ImGuiKey_U;
            case 'V':           return ImGuiKey_V;
            case 'W':           return ImGuiKey_W;
            case 'X':           return ImGuiKey_X;
            case 'Y':           return ImGuiKey_Y;
            case 'Z':           return ImGuiKey_Z;
            case VK_F1:         return ImGuiKey_F1;
            case VK_F2:         return ImGuiKey_F2;
            case VK_F3:         return ImGuiKey_F3;
            case VK_F4:         return ImGuiKey_F4;
            case VK_F5:         return ImGuiKey_F5;
            case VK_F6:         return ImGuiKey_F6;
            case VK_F7:         return ImGuiKey_F7;
            case VK_F8:         return ImGuiKey_F8;
            case VK_F9:         return ImGuiKey_F9;
            case VK_F10:        return ImGuiKey_F10;
            case VK_F11:        return ImGuiKey_F11;
            case VK_F12:        return ImGuiKey_F12;
            default:            return ImGuiKey_None;
        }
    }

    void update_input()
    {
        ImGuiIO& io = ImGui::GetIO();

        // --- Mouse ---
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(overlay_hwnd, &p);
        io.MousePos = ImVec2((float)p.x, (float)p.y);
        io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

        // --- Build keyboard state from GetAsyncKeyState ---
        BYTE async_keyboard_state[256] = {};
        for (int i = 0; i < 256; i++)
        {
            if (GetAsyncKeyState(i) & 0x8000)
                async_keyboard_state[i] = 0x80;
        }
        // CapsLock / NumLock / ScrollLock toggle state
        if (GetKeyState(VK_CAPITAL) & 1)  async_keyboard_state[VK_CAPITAL] |= 0x01;
        if (GetKeyState(VK_NUMLOCK) & 1)  async_keyboard_state[VK_NUMLOCK] |= 0x01;
        if (GetKeyState(VK_SCROLL) & 1)   async_keyboard_state[VK_SCROLL]  |= 0x01;

        // --- Modifiers ---
        bool ctrl  = (async_keyboard_state[VK_CONTROL] & 0x80) != 0;
        bool shift = (async_keyboard_state[VK_SHIFT]   & 0x80) != 0;
        bool alt   = (async_keyboard_state[VK_MENU]    & 0x80) != 0;

        io.AddKeyEvent(ImGuiMod_Ctrl,  ctrl);
        io.AddKeyEvent(ImGuiMod_Shift, shift);
        io.AddKeyEvent(ImGuiMod_Alt,   alt);

        // --- Key events + character input ---
        static bool prev_key_state[256] = {};

        for (int vk = 0; vk < 256; vk++)
        {
            bool is_down = (async_keyboard_state[vk] & 0x80) != 0;
            bool was_down = prev_key_state[vk];

            ImGuiKey imgui_key = VirtualKeyToImGuiKey(vk);

            if (is_down != was_down)
            {
                // Send key event
                if (imgui_key != ImGuiKey_None)
                    io.AddKeyEvent(imgui_key, is_down);

                // Send character on key DOWN only
                if (is_down && !ctrl && !alt)
                {
                    // Skip modifier VKs
                    if (vk != VK_CONTROL && vk != VK_SHIFT && vk != VK_MENU &&
                        vk != VK_LCONTROL && vk != VK_RCONTROL &&
                        vk != VK_LSHIFT && vk != VK_RSHIFT &&
                        vk != VK_LMENU && vk != VK_RMENU &&
                        vk != VK_LWIN && vk != VK_RWIN)
                    {
                        wchar_t chars[5] = {};
                        UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
                        int ret = ToUnicode(vk, scan, async_keyboard_state, chars, 4, 0);

                        // ToUnicode can return -1 for dead keys, handle it
                        if (ret == -1)
                        {
                            // Dead key - call ToUnicode again to clear internal state
                            ToUnicode(vk, scan, async_keyboard_state, chars, 4, 0);
                        }
                        else if (ret > 0)
                        {
                            for (int i = 0; i < ret; i++)
                            {
                                if (chars[i] >= 32) // skip control characters
                                    io.AddInputCharacterUTF16(chars[i]);
                            }
                        }
                    }
                }
            }

            prev_key_state[vk] = is_down;
        }
    }

    // -------------------------------------------------------------------------
    void update_window_tracking() {
        if (using_discord_overlay)
            return;

        bool game_visible = !IsIconic(game_hwnd);
        HWND fg           = GetForegroundWindow();
        bool should_show  = game_visible && (fg == game_hwnd || fg == overlay_hwnd);

        if (!should_show) {
            if (was_visible) {
                win32k_.show_window(overlay_hwnd, SW_HIDE);
                was_visible = false;
            }
        } else {
            if (!was_visible) {
                win32k_.show_window(overlay_hwnd, SW_SHOWNOACTIVATE);
                was_visible = true;
            }
            RECT gr;
            GetWindowRect(game_hwnd, &gr);
            if (gr.left   != last_game_rect.left  || gr.top    != last_game_rect.top ||
                gr.right  != last_game_rect.right  || gr.bottom != last_game_rect.bottom) {
                last_game_rect = gr;
                win32k_.set_window_pos(overlay_hwnd, HWND_TOPMOST,
                             gr.left, gr.top,
                             gr.right - gr.left, gr.bottom - gr.top,
                             SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOREDRAW);
            }
        }
    }

    bool init_dx11() {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate = {0, 1};
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = overlay_hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        D3D_FEATURE_LEVEL level;
        if (FAILED(D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION,
                &sd, &swap_chain, &device, &level, &context)))
            return false;

        ID3D11Texture2D* bb;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&bb));
        device->CreateRenderTargetView(bb, nullptr, &rtv);
        bb->Release();
        return true;
    }

    // -------------------------------------------------------------------------
    void init_imgui() {
        ImGui::CreateContext();
        auto& io    = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui_ImplWin32_Init(overlay_hwnd);
        ImGui_ImplDX11_Init(device, context);

        rebuild_fonts();
        apply_menu_style();
    }

    // -------------------------------------------------------------------------
    void scan_fonts() {
        available_fonts.clear();
        menu_fonts.clear();

        char font_dir[MAX_PATH];
        GetWindowsDirectoryA(font_dir, MAX_PATH);
        std::string fd = std::string(font_dir) + "\\Fonts\\";

        struct LocalFont { const char* display; const char* path; };

        struct SysFont { const char* display; const char* filename; };
        static const SysFont menu_sys[] = {
            { "Tahoma",         "tahoma.ttf"         },
            { "Tahoma Bold",    "tahomabd.ttf"       },
            { "Verdana",        "verdana.ttf"        },
            { "Arial",          "arial.ttf"          },
        };
        for (const auto& ms : menu_sys) {
            std::string full = fd + ms.filename;
            if (file_exists(full.c_str())) menu_fonts.push_back({ ms.display, full });
        }
        if (menu_fonts.empty()) menu_fonts.push_back({ "Default (ImGui)", "" });

        if (g_settings.menu_font_index < 0) {
            g_settings.menu_font_index = 0;
            for (int i = 0; i < (int)menu_fonts.size(); i++) {
                if (menu_fonts[i].display_name == "Tahoma") {
                    g_settings.menu_font_index = i;
                    break;
                }
            }
        }
        if (g_settings.menu_font_index >= (int)menu_fonts.size())
            g_settings.menu_font_index = 0;

        // --- ESP fonts ---
        static const SysFont esp_sys[] = {
            { "Tahoma",           "tahoma.ttf"    },
            { "Tahoma Bold",      "tahomabd.ttf"  },
            { "Arial",            "arial.ttf"     },
            { "Arial Bold",       "arialbd.ttf"   },
            { "Arial Unicode MS", "ARIALUNI.TTF"  },
            { "Calibri",          "calibri.ttf"   },
            { "Calibri Bold",     "calibrib.ttf"  },
            { "Consolas",         "consola.ttf"   },
            { "Consolas Bold",    "consolab.ttf"  },
            { "Courier New",      "cour.ttf"      },
            { "Lucida Console",   "lucon.ttf"     },
            { "Malgun Gothic",    "malgun.ttf"    },
            { "Microsoft YaHei",  "msyh.ttc"      },
            { "Segoe UI",         "segoeui.ttf"   },
            { "Segoe UI Bold",    "seguisb.ttf"   },
            { "Trebuchet MS",     "trebuc.ttf"    },
            { "Verdana",          "verdana.ttf"   },
            { "Verdana Bold",     "verdanab.ttf"  },
        };

        for (const auto& es : esp_sys) {
            std::string full = fd + es.filename;
            if (file_exists(full.c_str())) available_fonts.push_back({ es.display, full });
        }
        if (available_fonts.empty()) available_fonts.push_back({ "Default (ImGui)", "" });

        if (g_settings.esp_font_index < 0) {
            g_settings.esp_font_index = 0;
            for (int i = 0; i < (int)available_fonts.size(); i++) {
                if (available_fonts[i].display_name == "Tahoma") {
                    g_settings.esp_font_index = i;
                    break;
                }
            }
        }
        if (g_settings.esp_font_index >= (int)available_fonts.size())
            g_settings.esp_font_index = 0;
    }

    // -------------------------------------------------------------------------
    const char* get_esp_font_path() {
        if (g_settings.esp_font_index >= 0 &&
            g_settings.esp_font_index < (int)available_fonts.size() &&
            !available_fonts[g_settings.esp_font_index].path.empty())
            return available_fonts[g_settings.esp_font_index].path.c_str();
        return nullptr;
    }

    const char* get_menu_font_path() {
        if (g_settings.menu_font_index >= 0 &&
            g_settings.menu_font_index < (int)menu_fonts.size() &&
            !menu_fonts[g_settings.menu_font_index].path.empty())
            return menu_fonts[g_settings.menu_font_index].path.c_str();
        return nullptr;
    }

    // -------------------------------------------------------------------------
    void force_foreground(HWND hwnd) {
        HWND  fg_hwnd   = GetForegroundWindow();
        DWORD fg_thread = GetWindowThreadProcessId(fg_hwnd, nullptr);
        DWORD our_thread= GetCurrentThreadId();

        if (fg_thread != our_thread) {
            win32k_.attach_thread_input(our_thread, fg_thread, TRUE);
            win32k_.set_foreground_window(hwnd);
            win32k_.set_focus(hwnd);
            win32k_.attach_thread_input(our_thread, fg_thread, FALSE);
        } else {
            win32k_.set_foreground_window(hwnd);
            win32k_.set_focus(hwnd);
        }
    }

    static const ImWchar* get_glyph_ranges() {
        static const ImWchar ranges[] = {
            0x0020, 0x00FF,  // Basic Latin + Latin Supplement
            0x0100, 0x024F,  // Latin Extended
            0x0370, 0x03FF,  // Greek
            0x0400, 0x052F,  // Cyrillic
            0x0600, 0x06FF,  // Arabic
            0x0E00, 0x0E7F,  // Thai
            0x1100, 0x11FF,  // Hangul Jamo
            0x2000, 0x206F,  // General Punctuation
            0x2100, 0x214F,  // Letterlike Symbols
            0x3000, 0x30FF,  // CJK / Hiragana / Katakana
            0x3130, 0x318F,  // Hangul Compatibility
            0x4E00, 0x9FAF,  // CJK Unified Ideographs
            0xAC00, 0xD7A3,  // Hangul Syllables
            0xFF00, 0xFFEF,  // Halfwidth / Fullwidth
            0,
        };
        return ranges;
    }

    static const char* find_system_font(const char* filename) {
        static std::string path;
        char font_dir[MAX_PATH];
        GetWindowsDirectoryA(font_dir, MAX_PATH);
        path = std::string(font_dir) + "\\Fonts\\" + filename;
        FILE* f = fopen(path.c_str(), "rb");
        if (f) { fclose(f); return path.c_str(); }
        return nullptr;
    }

    static bool file_exists(const char* p) {
        FILE* f = fopen(p, "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

    // -------------------------------------------------------------------------
    static LRESULT WINAPI wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 0;
        if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(h, m, w, l);
    }
};

inline Overlay g_overlay;