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
        game_hwnd = FindWindowW(nullptr, target_window);
        if (!game_hwnd) return false;

        RECT rc;
        GetClientRect(game_hwnd, &rc);
        width  = rc.right;
        height = rc.bottom;

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

        // THE MAGIC COMBO:
        // WS_EX_LAYERED + WS_EX_TRANSPARENT  → flawless system-wide click-through.
        // WS_EX_NOREDIRECTIONBITMAP           → prevents black screen, keeps FLIP_DISCARD zero-copy.
        overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
            WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, width, height,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!overlay_hwnd) return false;

        // Required to validate WS_EX_LAYERED for Windows input hit-testing.
        SetLayeredWindowAttributes(overlay_hwnd, 0, 255, LWA_ALPHA);

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(overlay_hwnd, &margins);

        BOOL disable_transitions = TRUE;
        DwmSetWindowAttribute(overlay_hwnd, DWMWA_TRANSITIONS_FORCEDISABLED,
                              &disable_transitions, sizeof(disable_transitions));

        RECT game_rect;
        GetWindowRect(game_hwnd, &game_rect);
        SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                     game_rect.left, game_rect.top, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);

        if (!init_dx11()) return false;

        scan_fonts();
        init_imgui();

        ShowWindow(overlay_hwnd, SW_SHOWNOACTIVATE);
        return true;
    }

    // -------------------------------------------------------------------------
    // Called when the user opens/closes the in-game menu.
    // interactive=true  → steals focus + cursor so ImGui widgets are clickable.
    // interactive=false → returns focus + cursor to the game.
    // -------------------------------------------------------------------------
    void set_interactive(bool interactive) {
        LONG ex = GetWindowLongW(overlay_hwnd, GWL_EXSTYLE);
        if (interactive) {
            // Remove click-through so overlay receives mouse input.
            if (ex & WS_EX_TRANSPARENT)
                SetWindowLongW(overlay_hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);

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
            // Restore click-through so the game gets all mouse input.
            ClipCursor(nullptr);
            if (!(ex & WS_EX_TRANSPARENT))
                SetWindowLongW(overlay_hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);

            while (ShowCursor(FALSE) >= 0) {}
            if (game_hwnd && IsWindow(game_hwnd)) {
                force_foreground(game_hwnd);
                PostMessage(game_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
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

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;

        if (mf_path) {
            default_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size, &cfg, io.Fonts->GetGlyphRangesDefault());
            menu_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size - 1.0f, &cfg, io.Fonts->GetGlyphRangesDefault());
            menu_title_font = io.Fonts->AddFontFromFileTTF(
                mf_path, mf_size + 4.0f, &cfg, io.Fonts->GetGlyphRangesDefault());
        }
        if (!default_font)    default_font    = io.Fonts->AddFontDefault();
        if (!menu_font)       menu_font       = default_font;
        if (!menu_title_font) menu_title_font = default_font;

        ImFontConfig spec_cfg;
        spec_cfg.OversampleH = 3;
        spec_cfg.OversampleV = 2;
        if (mf_path)
            spec_font = io.Fonts->AddFontFromFileTTF(mf_path, 13.0f, &spec_cfg, get_glyph_ranges());
        if (!spec_font) spec_font = io.Fonts->AddFontDefault();

        // ---- ESP fonts: each baked at its own render size for 1:1 pixel output ----
        const char* esp_path = get_esp_font_path();
        const char* esp_fb   = find_system_font("arial.ttf");

        auto build_esp_font = [&](float size) -> ImFont* {
            size = std::max(size, 8.0f);
            ImFontConfig c;
            c.OversampleH = (size <= 14.0f) ? 8 : 4;
            c.OversampleV = (size <= 14.0f) ? 8 : 4;
            c.PixelSnapH  = true;
            ImFont* f = nullptr;
            if (esp_path)
                f = io.Fonts->AddFontFromFileTTF(esp_path, size, &c, get_glyph_ranges());
            if (!f && esp_fb)
                f = io.Fonts->AddFontFromFileTTF(esp_fb,   size, &c, get_glyph_ranges());
            return f;
        };

        esp_font_name   = build_esp_font(g_settings.name_font_size);
        esp_font_hp     = build_esp_font(g_settings.hp_font_size);
        esp_font_weapon = build_esp_font(g_settings.weapon_font_size);
        esp_font_nade   = build_esp_font(g_settings.grenade_text_font_size);

        // esp_font points to name font as general fallback
        esp_font = esp_font_name;
        if (!esp_font)        esp_font        = io.Fonts->AddFontDefault();
        if (!esp_font_hp)     esp_font_hp     = esp_font;
        if (!esp_font_weapon) esp_font_weapon = esp_font;
        if (!esp_font_nade)   esp_font_nade   = esp_font;

        // atlas_size is now just name_font_size (used by any legacy callers)
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
        ImGui::NewFrame();
        return true;
    }

    // -------------------------------------------------------------------------
    void end_frame(int sync_interval) {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        // WAITABLE SWAPCHAIN: skip GPU work when DWM queue is full.
        // CPU loop keeps running at full speed; GPU only draws at monitor refresh rate.
        if (frame_latency_waitable_object) {
            if (WaitForSingleObject(frame_latency_waitable_object, 0) == WAIT_TIMEOUT)
                return;
        }

        context->OMSetRenderTargets(1, &rtv, nullptr);
        const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
        context->ClearRenderTargetView(rtv, clear);

        if (draw_data && draw_data->TotalVtxCount > 0)
            ImGui_ImplDX11_RenderDrawData(draw_data);

        swap_chain->Present(sync_interval, 0);
    }

    // -------------------------------------------------------------------------
    void shutdown() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (frame_latency_waitable_object) {
            CloseHandle(frame_latency_waitable_object);
            frame_latency_waitable_object = nullptr;
        }
        if (dcomp_visual) { dcomp_visual->Release(); dcomp_visual = nullptr; }
        if (dcomp_target) { dcomp_target->Release(); dcomp_target = nullptr; }
        if (dcomp_device) { dcomp_device->Release(); dcomp_device = nullptr; }
        if (rtv)          { rtv->Release();          rtv          = nullptr; }
        if (swap_chain)   { swap_chain->Release();   swap_chain   = nullptr; }
        if (context)      { context->Release();      context      = nullptr; }
        if (device)       { device->Release();       device       = nullptr; }
        if (overlay_hwnd) { DestroyWindow(overlay_hwnd); overlay_hwnd = nullptr; }
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

    // DX11 + DComp objects
    ID3D11Device*        device       = nullptr;
    ID3D11DeviceContext* context      = nullptr;
    IDXGISwapChain2*     swap_chain   = nullptr;   // IDXGISwapChain2 for waitable object
    ID3D11RenderTargetView* rtv       = nullptr;
    HANDLE frame_latency_waitable_object = nullptr;

    IDCompositionDevice* dcomp_device = nullptr;
    IDCompositionTarget* dcomp_target = nullptr;
    IDCompositionVisual* dcomp_visual = nullptr;

    bool was_visible = true;
    RECT last_game_rect{};

    // -------------------------------------------------------------------------
    void update_window_tracking() {
        bool game_visible = !IsIconic(game_hwnd);
        HWND fg           = GetForegroundWindow();
        bool should_show  = game_visible && (fg == game_hwnd || fg == overlay_hwnd);

        if (!should_show && !g_settings.menu_open) {
            if (was_visible) {
                ShowWindow(overlay_hwnd, SW_HIDE);
                was_visible = false;
            }
        } else {
            if (!was_visible) {
                ShowWindow(overlay_hwnd, SW_SHOWNOACTIVATE);
                was_visible = true;
            }
            RECT gr;
            GetWindowRect(game_hwnd, &gr);
            if (gr.left   != last_game_rect.left  || gr.top    != last_game_rect.top ||
                gr.right  != last_game_rect.right  || gr.bottom != last_game_rect.bottom) {
                last_game_rect = gr;
                SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                             gr.left, gr.top,
                             gr.right - gr.left, gr.bottom - gr.top,
                             SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOREDRAW);
            }
        }
    }

    // -------------------------------------------------------------------------
    bool init_dx11() {
        UINT device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL feature_level;

        if (FAILED(D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                device_flags, nullptr, 0, D3D11_SDK_VERSION,
                &device, &feature_level, &context)))
            return false;

        IDXGIDevice*   dxgi_device  = nullptr;
        IDXGIAdapter*  dxgi_adapter = nullptr;
        IDXGIFactory2* dxgi_factory = nullptr;

        device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
        dxgi_device->GetAdapter(&dxgi_adapter);
        dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width           = width;
        sd.Height          = height;
        sd.Format          = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count= 1;
        sd.BufferUsage     = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount     = 2;
        sd.SwapEffect      = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode       = DXGI_ALPHA_MODE_PREMULTIPLIED;
        sd.Flags           = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        IDXGISwapChain1* swap_chain1 = nullptr;
        HRESULT hr = dxgi_factory->CreateSwapChainForComposition(
            device, &sd, nullptr, &swap_chain1);

        dxgi_factory->Release();
        dxgi_adapter->Release();
        dxgi_device->Release();

        if (FAILED(hr)) return false;

        hr = swap_chain1->QueryInterface(IID_PPV_ARGS(&swap_chain));
        swap_chain1->Release();
        if (FAILED(hr)) return false;

        swap_chain->SetMaximumFrameLatency(1);
        frame_latency_waitable_object = swap_chain->GetFrameLatencyWaitableObject();

        if (!bind_swap_chain_to_window(swap_chain)) return false;

        ID3D11Texture2D* back_buffer = nullptr;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        device->CreateRenderTargetView(back_buffer, nullptr, &rtv);
        back_buffer->Release();

        D3D11_VIEWPORT vp{};
        vp.Width    = static_cast<float>(width);
        vp.Height   = static_cast<float>(height);
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        return true;
    }

    // -------------------------------------------------------------------------
    bool bind_swap_chain_to_window(IDXGISwapChain1* sc) {
        IDXGIDevice* dxgi_dev = nullptr;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi_dev)))) return false;

        HRESULT hr = DCompositionCreateDevice(dxgi_dev, IID_PPV_ARGS(&dcomp_device));
        dxgi_dev->Release();
        if (FAILED(hr)) return false;

        if (FAILED(dcomp_device->CreateTargetForHwnd(overlay_hwnd, TRUE, &dcomp_target))) return false;
        if (FAILED(dcomp_device->CreateVisual(&dcomp_visual)))                             return false;
        if (FAILED(dcomp_visual->SetContent(sc)))                                          return false;
        if (FAILED(dcomp_target->SetRoot(dcomp_visual)))                                   return false;
        if (FAILED(dcomp_device->Commit()))                                                return false;

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
        static const LocalFont fira_variants[] = {
            { "Fira Code Light",    "fonts/FiraCode-Light.ttf"    },
            { "Fira Code Regular",  "fonts/FiraCode-Regular.ttf"  },
            { "Fira Code Medium",   "fonts/FiraCode-Medium.ttf"   },
            { "Fira Code SemiBold", "fonts/FiraCode-SemiBold.ttf" },
            { "Fira Code Bold",     "fonts/FiraCode-Bold.ttf"     },
        };

        for (const auto& fv : fira_variants)
            if (file_exists(fv.path)) menu_fonts.push_back({ fv.display, fv.path });

        struct SysFont { const char* display; const char* filename; };
        static const SysFont menu_sys[] = {
            { "Consolas",       "consola.ttf"        },
            { "Consolas Bold",  "consolab.ttf"       },
            { "Segoe UI",       "segoeui.ttf"        },
            { "Segoe UI Bold",  "seguisb.ttf"        },
            { "Cascadia Code",  "CascadiaCode.ttf"   },
            { "Cascadia Mono",  "CascadiaMono.ttf"   },
            { "Lucida Console", "lucon.ttf"          },
            { "Courier New",    "cour.ttf"           },
            { "Tahoma",         "tahoma.ttf"         },
            { "Arial",          "arial.ttf"          },
            { "Verdana",        "verdana.ttf"        },
        };
        for (const auto& ms : menu_sys) {
            std::string full = fd + ms.filename;
            if (file_exists(full.c_str())) menu_fonts.push_back({ ms.display, full });
        }
        if (menu_fonts.empty()) menu_fonts.push_back({ "Default (ImGui)", "" });

        if (g_settings.menu_font_index < 0 ||
            g_settings.menu_font_index >= (int)menu_fonts.size())
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

        for (const auto& fv : fira_variants)
            if (file_exists(fv.path)) available_fonts.push_back({ fv.display, fv.path });

        for (const auto& es : esp_sys) {
            std::string full = fd + es.filename;
            if (file_exists(full.c_str())) available_fonts.push_back({ es.display, full });
        }
        if (available_fonts.empty()) available_fonts.push_back({ "Default (ImGui)", "" });

        // Auto-select Tahoma for ESP on first launch; clamp on subsequent launches.
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
    static void force_foreground(HWND hwnd) {
        HWND  fg_hwnd   = GetForegroundWindow();
        DWORD fg_thread = GetWindowThreadProcessId(fg_hwnd, nullptr);
        DWORD our_thread= GetCurrentThreadId();
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