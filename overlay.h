#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "settings.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct FontEntry {
    std::string display_name;
    std::string path;
};

class Overlay {
public:
    HWND overlay_hwnd = nullptr;
    HWND game_hwnd = nullptr;
    int width = 0, height = 0;

    ImFont* default_font = nullptr;
    ImFont* esp_font = nullptr;
    ImFont* spec_font = nullptr;
    ImFont* menu_font = nullptr;
    ImFont* menu_title_font = nullptr;

    std::vector<FontEntry> available_fonts;
    std::vector<FontEntry> menu_fonts;
    bool font_rebuild_needed = false;

    ID3D11Device* get_device() const { return device; }

    bool init(const wchar_t* target_window) {
        game_hwnd = FindWindowW(nullptr, target_window);
        if (!game_hwnd) return false;

        RECT rc;
        GetClientRect(game_hwnd, &rc);
        width = rc.right;
        height = rc.bottom;

        wchar_t class_name[16];
        srand((unsigned)GetTickCount64());
        for (int i = 0; i < 12; i++)
            class_name[i] = L'a' + (rand() % 26);
        class_name[12] = 0;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = class_name;
        RegisterClassExW(&wc);

        overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, width, height,
            nullptr, nullptr, wc.hInstance, nullptr);

        SetLayeredWindowAttributes(overlay_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
        MARGINS margins = {-1};
        DwmExtendFrameIntoClientArea(overlay_hwnd, &margins);

        RECT game_rect;
        GetWindowRect(game_hwnd, &game_rect);
        SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                     game_rect.left, game_rect.top, width, height, SWP_SHOWWINDOW);

        if (!init_dx11()) return false;

        scan_fonts();
        init_imgui();

        ShowWindow(overlay_hwnd, SW_SHOWDEFAULT);
        return true;
    }

    void set_interactive(bool interactive) {
        LONG ex = GetWindowLong(overlay_hwnd, GWL_EXSTYLE);
        if (interactive) {
            ex &= ~WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);
            force_foreground(overlay_hwnd);
            while (ShowCursor(TRUE) < 0) {}
            Sleep(50);
            RECT wr;
            GetWindowRect(overlay_hwnd, &wr);
            SetCursorPos((wr.left + wr.right) / 2, (wr.top + wr.bottom) / 2);
            ClipCursor(&wr);
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(overlay_hwnd, &cursor);
            PostMessage(overlay_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(cursor.x, cursor.y));
            PostMessage(overlay_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
        } else {
            ClipCursor(nullptr);
            ex |= WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);
            while (ShowCursor(FALSE) >= 0) {}
            if (game_hwnd && IsWindow(game_hwnd)) {
                force_foreground(game_hwnd);
                PostMessage(game_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
            }
        }
    }

    void rebuild_fonts() {
        auto& io = ImGui::GetIO();
        ImGui_ImplDX11_InvalidateDeviceObjects();
        io.Fonts->Clear();

        const char* mf_path = get_menu_font_path();
        float mf_size = g_settings.menu_font_size;

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;

        if (mf_path) {
            default_font = io.Fonts->AddFontFromFileTTF(mf_path, mf_size, &cfg,
                io.Fonts->GetGlyphRangesDefault());
            menu_font = io.Fonts->AddFontFromFileTTF(mf_path, mf_size - 1.0f, &cfg,
                io.Fonts->GetGlyphRangesDefault());
            menu_title_font = io.Fonts->AddFontFromFileTTF(mf_path, mf_size + 4.0f, &cfg,
                io.Fonts->GetGlyphRangesDefault());
        }
        if (!default_font) default_font = io.Fonts->AddFontDefault();
        if (!menu_font) menu_font = default_font;
        if (!menu_title_font) menu_title_font = default_font;

        ImFontConfig spec_cfg;
        spec_cfg.OversampleH = 2;
        spec_cfg.OversampleV = 1;
        if (mf_path)
            spec_font = io.Fonts->AddFontFromFileTTF(mf_path, 13.0f, &spec_cfg, get_glyph_ranges());
        if (!spec_font) spec_font = io.Fonts->AddFontDefault();

        float atlas_size = std::max({g_settings.name_font_size, g_settings.hp_font_size,
                                     g_settings.weapon_font_size, 14.0f});
        atlas_size = std::min(atlas_size + 4.0f, 32.0f);
        g_settings.esp_font_atlas_size = atlas_size;

        const char* esp_path = get_esp_font_path();
        ImFontConfig esp_cfg;
        esp_cfg.OversampleH = 2;
        esp_cfg.OversampleV = 1;

        esp_font = nullptr;
        if (esp_path)
            esp_font = io.Fonts->AddFontFromFileTTF(esp_path, atlas_size, &esp_cfg, get_glyph_ranges());
        if (!esp_font) {
            const char* fb = find_system_font("arial.ttf");
            if (fb) esp_font = io.Fonts->AddFontFromFileTTF(fb, atlas_size, &esp_cfg, get_glyph_ranges());
        }
        if (!esp_font) esp_font = io.Fonts->AddFontDefault();

        io.Fonts->Build();
        ImGui_ImplDX11_CreateDeviceObjects();
        font_rebuild_needed = false;
    }

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
            bool game_visible = !IsIconic(game_hwnd);
            HWND fg = GetForegroundWindow();
            bool should_show = game_visible && (fg == game_hwnd || fg == overlay_hwnd);
            if (!should_show && !g_settings.menu_open) {
                ShowWindow(overlay_hwnd, SW_HIDE);
            } else {
                if (!IsWindowVisible(overlay_hwnd))
                    ShowWindow(overlay_hwnd, SW_SHOWNOACTIVATE);
                RECT gr;
                GetWindowRect(game_hwnd, &gr);
                SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                             gr.left, gr.top, gr.right - gr.left, gr.bottom - gr.top,
                             SWP_NOACTIVATE);
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        return true;
    }

    void end_frame(int sync_interval) {
        ImGui::Render();
        const float clear[4] = {0, 0, 0, 0};
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap_chain->Present(sync_interval, 0);
    }

    void shutdown() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (rtv) rtv->Release();
        if (swap_chain) swap_chain->Release();
        if (context) context->Release();
        if (device) device->Release();
        DestroyWindow(overlay_hwnd);
    }

    void apply_menu_style() {
        auto& s = ImGui::GetStyle();
        ImVec4* colors = s.Colors;
        ImVec4 accent = {g_settings.menu_accent_color[0], g_settings.menu_accent_color[1],
                         g_settings.menu_accent_color[2], g_settings.menu_accent_color[3]};
        ImVec4 accent_dim = {accent.x * 0.6f, accent.y * 0.6f, accent.z * 0.6f, accent.w * 0.7f};
        ImVec4 border = {g_settings.menu_border_color[0], g_settings.menu_border_color[1],
                         g_settings.menu_border_color[2], g_settings.menu_border_color[3]};
        float bg_a = g_settings.menu_bg_alpha;

        colors[ImGuiCol_WindowBg]         = {0.06f, 0.06f, 0.08f, bg_a};
        colors[ImGuiCol_ChildBg]          = {0.07f, 0.07f, 0.09f, bg_a * 0.5f};
        colors[ImGuiCol_PopupBg]          = {0.08f, 0.08f, 0.10f, bg_a};
        colors[ImGuiCol_Border]           = border;
        colors[ImGuiCol_BorderShadow]     = {0, 0, 0, 0};
        colors[ImGuiCol_FrameBg]          = {0.10f, 0.10f, 0.12f, 0.8f};
        colors[ImGuiCol_FrameBgHovered]   = {accent.x * 0.2f, accent.y * 0.2f, accent.z * 0.2f, 0.6f};
        colors[ImGuiCol_FrameBgActive]    = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.8f};
        colors[ImGuiCol_TitleBg]          = {0.04f, 0.04f, 0.06f, bg_a};
        colors[ImGuiCol_TitleBgActive]    = {accent.x * 0.1f, accent.y * 0.1f, accent.z * 0.1f, bg_a};
        colors[ImGuiCol_TitleBgCollapsed] = {0.04f, 0.04f, 0.06f, 0.5f};
        colors[ImGuiCol_Tab]             = {0.08f, 0.08f, 0.10f, 0.8f};
        colors[ImGuiCol_TabHovered]      = {accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.8f};
        colors[ImGuiCol_TabActive]       = {accent.x * 0.2f, accent.y * 0.2f, accent.z * 0.2f, 1.0f};
        colors[ImGuiCol_TabUnfocused]    = {0.06f, 0.06f, 0.08f, 0.8f};
        colors[ImGuiCol_TabUnfocusedActive] = {accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.9f};
        colors[ImGuiCol_Button]          = {0.12f, 0.12f, 0.14f, 0.8f};
        colors[ImGuiCol_ButtonHovered]   = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.8f};
        colors[ImGuiCol_ButtonActive]    = {accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 1.0f};
        colors[ImGuiCol_Header]          = {accent.x * 0.15f, accent.y * 0.15f, accent.z * 0.15f, 0.6f};
        colors[ImGuiCol_HeaderHovered]   = {accent.x * 0.25f, accent.y * 0.25f, accent.z * 0.25f, 0.8f};
        colors[ImGuiCol_HeaderActive]    = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f};
        colors[ImGuiCol_SliderGrab]      = accent_dim;
        colors[ImGuiCol_SliderGrabActive]= accent;
        colors[ImGuiCol_CheckMark]       = accent;
        colors[ImGuiCol_Separator]       = {accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.5f};
        colors[ImGuiCol_SeparatorHovered]= accent_dim;
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_ScrollbarBg]     = {0.05f, 0.05f, 0.07f, 0.5f};
        colors[ImGuiCol_ScrollbarGrab]   = {0.15f, 0.15f, 0.18f, 0.8f};
        colors[ImGuiCol_ScrollbarGrabHovered] = accent_dim;
        colors[ImGuiCol_ScrollbarGrabActive]  = accent;
        colors[ImGuiCol_Text]            = {0.85f, 0.90f, 0.88f, 1.0f};
        colors[ImGuiCol_TextDisabled]    = {0.40f, 0.45f, 0.43f, 1.0f};
        colors[ImGuiCol_ResizeGrip]      = {accent.x * 0.2f, accent.y * 0.2f, accent.z * 0.2f, 0.3f};
        colors[ImGuiCol_ResizeGripHovered]= accent_dim;
        colors[ImGuiCol_ResizeGripActive] = accent;

        s.WindowRounding = 4.0f;
        s.FrameRounding = 3.0f;
        s.GrabRounding = 2.0f;
        s.TabRounding = 3.0f;
        s.ScrollbarRounding = 2.0f;
        s.WindowBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 1.0f;
        s.WindowPadding = {10, 10};
        s.FramePadding = {6, 4};
        s.ItemSpacing = {8, 6};
        s.ItemInnerSpacing = {6, 4};
        s.Alpha = 1.0f;
    }

private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    static void force_foreground(HWND hwnd) {
        HWND current_fg = GetForegroundWindow();
        DWORD fg_thread = GetWindowThreadProcessId(current_fg, nullptr);
        DWORD our_thread = GetCurrentThreadId();
        if (fg_thread != our_thread) {
            AttachThreadInput(our_thread, fg_thread, TRUE);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            BringWindowToTop(hwnd);
            AttachThreadInput(our_thread, fg_thread, FALSE);
        } else {
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            BringWindowToTop(hwnd);
        }
    }

    static const ImWchar* get_glyph_ranges() {
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, 0x0100, 0x024F, 0x0370, 0x03FF,
            0x0400, 0x052F, 0x0600, 0x06FF, 0x0E00, 0x0E7F,
            0x1100, 0x11FF, 0x2000, 0x206F, 0x2100, 0x214F,
            0x3000, 0x30FF, 0x3130, 0x318F, 0x4E00, 0x9FAF,
            0xAC00, 0xD7A3, 0xFF00, 0xFFEF, 0,
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

    void scan_fonts() {
        available_fonts.clear();
        menu_fonts.clear();

        char font_dir[MAX_PATH];
        GetWindowsDirectoryA(font_dir, MAX_PATH);
        std::string fd = std::string(font_dir) + "\\Fonts\\";

        struct LocalFont { const char* display; const char* path; };
        static const LocalFont fira_variants[] = {
            {"Fira Code Light",    "fonts/FiraCode-Light.ttf"},
            {"Fira Code Regular",  "fonts/FiraCode-Regular.ttf"},
            {"Fira Code Medium",   "fonts/FiraCode-Medium.ttf"},
            {"Fira Code SemiBold", "fonts/FiraCode-SemiBold.ttf"},
            {"Fira Code Bold",     "fonts/FiraCode-Bold.ttf"},
        };
        for (const auto& fv : fira_variants) {
            if (file_exists(fv.path))
                menu_fonts.push_back({fv.display, fv.path});
        }

        struct SysFont { const char* display; const char* filename; };
        static const SysFont menu_sys[] = {
            {"Consolas",        "consola.ttf"},
            {"Consolas Bold",   "consolab.ttf"},
            {"Segoe UI",        "segoeui.ttf"},
            {"Segoe UI Bold",   "seguisb.ttf"},
            {"Cascadia Code",   "CascadiaCode.ttf"},
            {"Cascadia Mono",   "CascadiaMono.ttf"},
            {"Lucida Console",  "lucon.ttf"},
            {"Courier New",     "cour.ttf"},
            {"Tahoma",          "tahoma.ttf"},
            {"Arial",           "arial.ttf"},
            {"Verdana",         "verdana.ttf"},
        };
        for (const auto& ms : menu_sys) {
            std::string full = fd + ms.filename;
            if (file_exists(full.c_str()))
                menu_fonts.push_back({ms.display, full});
        }
        if (menu_fonts.empty())
            menu_fonts.push_back({"Default (ImGui)", ""});

        if (g_settings.menu_font_index < 0 ||
            g_settings.menu_font_index >= (int)menu_fonts.size())
            g_settings.menu_font_index = 0;

        static const SysFont esp_sys[] = {
            {"Tahoma",             "tahoma.ttf"},
            {"Tahoma Bold",        "tahomabd.ttf"},
            {"Arial",              "arial.ttf"},
            {"Arial Bold",         "arialbd.ttf"},
            {"Arial Unicode MS",   "ARIALUNI.TTF"},
            {"Calibri",            "calibri.ttf"},
            {"Calibri Bold",       "calibrib.ttf"},
            {"Consolas",           "consola.ttf"},
            {"Consolas Bold",      "consolab.ttf"},
            {"Courier New",        "cour.ttf"},
            {"Lucida Console",     "lucon.ttf"},
            {"Malgun Gothic",      "malgun.ttf"},
            {"Microsoft YaHei",    "msyh.ttc"},
            {"Segoe UI",           "segoeui.ttf"},
            {"Segoe UI Bold",      "seguisb.ttf"},
            {"Trebuchet MS",       "trebuc.ttf"},
            {"Verdana",            "verdana.ttf"},
            {"Verdana Bold",       "verdanab.ttf"},
        };


        for (const auto& fv : fira_variants) {
            if (file_exists(fv.path))
                available_fonts.push_back({fv.display, fv.path});
        }

        for (const auto& es : esp_sys) {
            std::string full = fd + es.filename;
            if (file_exists(full.c_str()))
                available_fonts.push_back({es.display, full});
        }
        if (available_fonts.empty())
            available_fonts.push_back({"Default (ImGui)", ""});

        // Auto-select: only run when esp_font_index == -1 (first ever launch / reset).
        // Search for Tahoma by display name; fall back to index 0.
        if (g_settings.esp_font_index < 0) {
            g_settings.esp_font_index = 0;  // safe fallback
            for (int i = 0; i < (int)available_fonts.size(); i++) {
                if (available_fonts[i].display_name == "Tahoma") {
                    g_settings.esp_font_index = i;
                    break;
                }
            }
        }
        // Clamp in case saved index is now out of range (fonts removed etc.)
        if (g_settings.esp_font_index >= (int)available_fonts.size())
            g_settings.esp_font_index = 0;
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

    void init_imgui() {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui_ImplWin32_Init(overlay_hwnd);
        ImGui_ImplDX11_Init(device, context);

        rebuild_fonts();
        apply_menu_style();
    }

    static LRESULT WINAPI wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 0;
        if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(h, m, w, l);
    }
};

inline Overlay g_overlay;