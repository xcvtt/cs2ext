#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <cstdio>
#include <cstdlib>
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

    std::vector<FontEntry> available_fonts;
    bool font_rebuild_needed = false;

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

        scan_system_fonts();
        init_imgui();

        ShowWindow(overlay_hwnd, SW_SHOWDEFAULT);
        return true;
    }

    void set_interactive(bool interactive) {
        LONG ex = GetWindowLong(overlay_hwnd, GWL_EXSTYLE);
        if (interactive) {
            ex &= ~WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);

            // Force foreground FIRST so game stops capturing mouse
            force_foreground(overlay_hwnd);

            // Show Windows cursor
            while (ShowCursor(TRUE) < 0) {}

            // Small delay to let game release mouse capture
            Sleep(50);

            // NOW center cursor after game has released input
            RECT wr;
            GetWindowRect(overlay_hwnd, &wr);
            int cx = (wr.left + wr.right) / 2;
            int cy = (wr.top + wr.bottom) / 2;
            SetCursorPos(cx, cy);

            // Clip cursor to overlay so game can't move it
            ClipCursor(&wr);

            // Tell ImGui the mouse position
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(overlay_hwnd, &cursor);
            PostMessage(overlay_hwnd, WM_MOUSEMOVE, 0,
                        MAKELPARAM(cursor.x, cursor.y));
            PostMessage(overlay_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
        } else {
            // Release cursor clip
            ClipCursor(nullptr);

            ex |= WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);

            // Hide Windows cursor
            while (ShowCursor(FALSE) >= 0) {}

            if (game_hwnd && IsWindow(game_hwnd)) {
                force_foreground(game_hwnd);
                PostMessage(game_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
            }
        }
    }

    void rebuild_esp_font() {
        auto& io = ImGui::GetIO();

        ImGui_ImplDX11_InvalidateDeviceObjects();
        io.Fonts->Clear();

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        default_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &cfg,
            io.Fonts->GetGlyphRangesDefault());
        if (!default_font)
            default_font = io.Fonts->AddFontDefault();

        ImFontConfig spec_cfg;
        spec_cfg.OversampleH = 2;
        spec_cfg.OversampleV = 1;
        spec_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 13.0f, &spec_cfg,
            get_glyph_ranges());
        if (!spec_font)
            spec_font = io.Fonts->AddFontDefault();

        float atlas_size = std::max({g_settings.name_font_size,
                                     g_settings.hp_font_size, 14.0f});
        atlas_size = std::min(atlas_size + 4.0f, 32.0f);
        g_settings.esp_font_atlas_size = atlas_size;

        const char* font_path = get_current_font_path();

        ImFontConfig esp_cfg;
        esp_cfg.OversampleH = 2;
        esp_cfg.OversampleV = 1;

        esp_font = nullptr;
        if (font_path) {
            esp_font = io.Fonts->AddFontFromFileTTF(
                font_path, atlas_size, &esp_cfg, get_glyph_ranges());
        }
        if (!esp_font)
            esp_font = io.Fonts->AddFontDefault();

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

        if (font_rebuild_needed)
            rebuild_esp_font();

        static int check_tick = 0;
        if (++check_tick >= 10) {
            check_tick = 0;
            bool game_visible = !IsIconic(game_hwnd);
            HWND fg = GetForegroundWindow();

            bool should_show = game_visible &&
                               (fg == game_hwnd || fg == overlay_hwnd);

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

    void end_frame() {
        ImGui::Render();
        const float clear[4] = {0, 0, 0, 0};
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap_chain->Present(0, 0);
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

private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    // Force a window to become the foreground window even from a background thread.
    // Windows normally blocks SetForegroundWindow from non-foreground threads.
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

    const char* get_current_font_path() {
        if (g_settings.esp_font_index >= 0 &&
            g_settings.esp_font_index < (int)available_fonts.size() &&
            !available_fonts[g_settings.esp_font_index].path.empty()) {
            return available_fonts[g_settings.esp_font_index].path.c_str();
        }
        return nullptr;
    }

    void scan_system_fonts() {
        available_fonts.clear();

        struct FontCandidate {
            const char* display;
            const char* filename;
        };
        static const FontCandidate candidates[] = {
            {"Arial",              "arial.ttf"},
            {"Arial Bold",         "arialbd.ttf"},
            {"Arial Unicode MS",   "ARIALUNI.TTF"},
            {"Calibri",            "calibri.ttf"},
            {"Calibri Bold",       "calibrib.ttf"},
            {"Consolas",           "consola.ttf"},
            {"Consolas Bold",      "consolab.ttf"},
            {"Courier New",        "cour.ttf"},
            {"Courier New Bold",   "courbd.ttf"},
            {"Lucida Console",     "lucon.ttf"},
            {"Malgun Gothic",      "malgun.ttf"},
            {"Malgun Gothic Bold", "malgunbd.ttf"},
            {"Microsoft YaHei",    "msyh.ttc"},
            {"Microsoft YaHei Bold","msyhbd.ttc"},
            {"Segoe UI",           "segoeui.ttf"},
            {"Segoe UI Bold",      "seguisb.ttf"},
            {"Tahoma",             "tahoma.ttf"},
            {"Tahoma Bold",        "tahomabd.ttf"},
            {"Times New Roman",    "times.ttf"},
            {"Trebuchet MS",       "trebuc.ttf"},
            {"Trebuchet MS Bold",  "trebucbd.ttf"},
            {"Verdana",            "verdana.ttf"},
            {"Verdana Bold",       "verdanab.ttf"},
        };

        char font_dir[MAX_PATH];
        GetWindowsDirectoryA(font_dir, MAX_PATH);
        strcat_s(font_dir, "\\Fonts\\");

        for (const auto& c : candidates) {
            std::string full_path = std::string(font_dir) + c.filename;
            FILE* f = fopen(full_path.c_str(), "rb");
            if (f) {
                fclose(f);
                available_fonts.push_back({c.display, full_path});
            }
        }

        if (available_fonts.empty())
            available_fonts.push_back({"Default (ImGui)", ""});

        if (g_settings.esp_font_index < 0 ||
            g_settings.esp_font_index >= (int)available_fonts.size())
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

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;

        default_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &cfg,
            io.Fonts->GetGlyphRangesDefault());
        if (!default_font)
            default_font = io.Fonts->AddFontDefault();

        ImFontConfig spec_cfg;
        spec_cfg.OversampleH = 2;
        spec_cfg.OversampleV = 1;
        spec_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 13.0f, &spec_cfg,
            get_glyph_ranges());
        if (!spec_font)
            spec_font = io.Fonts->AddFontDefault();

        float atlas_size = std::max({g_settings.name_font_size,
                                     g_settings.hp_font_size, 14.0f});
        atlas_size = std::min(atlas_size + 4.0f, 32.0f);
        g_settings.esp_font_atlas_size = atlas_size;

        ImFontConfig esp_cfg;
        esp_cfg.OversampleH = 2;
        esp_cfg.OversampleV = 1;

        const char* font_path = get_current_font_path();

        esp_font = nullptr;
        if (font_path) {
            esp_font = io.Fonts->AddFontFromFileTTF(
                font_path, atlas_size, &esp_cfg, get_glyph_ranges());
        }

        if (!esp_font) {
            const char* fallbacks[] = {
                "C:\\Windows\\Fonts\\ARIALUNI.TTF",
                "C:\\Windows\\Fonts\\msyh.ttc",
                "C:\\Windows\\Fonts\\arial.ttf",
                "C:\\Windows\\Fonts\\segoeui.ttf",
            };
            for (const char* fb : fallbacks) {
                FILE* f = fopen(fb, "rb");
                if (f) {
                    fclose(f);
                    esp_font = io.Fonts->AddFontFromFileTTF(
                        fb, atlas_size, &esp_cfg, get_glyph_ranges());
                    if (esp_font) break;
                }
            }
        }
        if (!esp_font)
            esp_font = io.Fonts->AddFontDefault();

        io.Fonts->Build();

        ImGui::StyleColorsDark();
        auto& s = ImGui::GetStyle();
        s.WindowRounding = 6.0f;
        s.FrameRounding = 4.0f;
        s.GrabRounding = 4.0f;
        s.WindowBorderSize = 1.0f;
        s.Alpha = 0.95f;

        ImGui_ImplWin32_Init(overlay_hwnd);
        ImGui_ImplDX11_Init(device, context);
    }

    static LRESULT WINAPI wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 0;
        if (m == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }
};

inline Overlay g_overlay;