#pragma once
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct MenuSettings;
extern MenuSettings g_settings;

class Menu;
extern Menu g_menu;

class Overlay {
public:
    HWND overlay_hwnd = nullptr;
    HWND game_hwnd = nullptr;
    int width = 0, height = 0;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    ImFont* default_font = nullptr;
    ImFont* esp_font = nullptr;

    bool init(const wchar_t* target_window) {
        game_hwnd = FindWindowW(nullptr, target_window);
        if (!game_hwnd) return false;

        RECT rc;
        GetClientRect(game_hwnd, &rc);
        width = rc.right; height = rc.bottom;

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
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(overlay_hwnd, &margins);

        RECT game_rect;
        GetWindowRect(game_hwnd, &game_rect);
        SetWindowPos(overlay_hwnd, HWND_TOPMOST,
            game_rect.left, game_rect.top, width, height, SWP_SHOWWINDOW);

        if (!init_dx11()) return false;
        init_imgui();
        ShowWindow(overlay_hwnd, SW_SHOWDEFAULT);
        return true;
    }

    void update_clickthrough(bool menu_open) {
        LONG ex = GetWindowLong(overlay_hwnd, GWL_EXSTYLE);
        if (menu_open) {
            ex &= ~WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);
            SetForegroundWindow(overlay_hwnd);
        } else {
            ex |= WS_EX_TRANSPARENT;
            SetWindowLong(overlay_hwnd, GWL_EXSTYLE, ex);
        }
    }

    bool begin_frame() {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(game_hwnd)) return false;

        static int repos = 0;
        if (++repos >= 30) {
            repos = 0;
            RECT gr;
            GetWindowRect(game_hwnd, &gr);
            SetWindowPos(overlay_hwnd, HWND_TOPMOST,
                gr.left, gr.top, gr.right-gr.left, gr.bottom-gr.top,
                SWP_NOACTIVATE);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        return true;
    }

    void end_frame() {
        ImGui::Render();
        const float clear[4] = {0,0,0,0};
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

        // Build font atlas with full Unicode glyph ranges
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;

        // Default UI font (menu)
        default_font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &cfg,
            io.Fonts->GetGlyphRangesDefault());

        // If Segoe UI not found, fall back
        if (!default_font)
            default_font = io.Fonts->AddFontDefault();

        // ESP font with full glyph ranges for UTF-8 names
        // Use a font that supports CJK, Cyrillic, etc.
        static const ImWchar glyph_ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0100, 0x024F, // Latin Extended-A/B
            0x0370, 0x03FF, // Greek
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x0600, 0x06FF, // Arabic
            0x0E00, 0x0E7F, // Thai
            0x1100, 0x11FF, // Hangul Jamo
            0x2000, 0x206F, // General Punctuation
            0x2100, 0x214F, // Letterlike Symbols
            0x3000, 0x30FF, // CJK Symbols, Hiragana, Katakana
            0x3130, 0x318F, // Hangul Compatibility Jamo
            0x4E00, 0x9FAF, // CJK Unified Ideographs (Chinese/Japanese)
            0xAC00, 0xD7A3, // Hangul Syllables (Korean)
            0xFF00, 0xFFEF, // Halfwidth/Fullwidth Forms
            0, // Terminator
        };

        ImFontConfig esp_cfg;
        esp_cfg.OversampleH = 2;
        esp_cfg.OversampleV = 1;

        // Try Arial Unicode MS first (has widest coverage), then others
        const char* font_paths[] = {
            "C:\\Windows\\Fonts\\ARIALUNI.TTF",   // Arial Unicode MS
            "C:\\Windows\\Fonts\\msyh.ttc",       // Microsoft YaHei (CJK)
            "C:\\Windows\\Fonts\\malgun.ttf",     // Malgun Gothic (Korean)
            "C:\\Windows\\Fonts\\arial.ttf",      // Arial (fallback)
            "C:\\Windows\\Fonts\\segoeui.ttf",    // Segoe UI (fallback)
        };

        esp_font = nullptr;
        for (const char* path : font_paths) {
            FILE* f = fopen(path, "rb");
            if (f) {
                fclose(f);
                esp_font = io.Fonts->AddFontFromFileTTF(
                    path, g_settings.esp_font_size, &esp_cfg, glyph_ranges);
                if (esp_font) {
                    printf("[+] ESP font: %s (%.0fpx)\n", path, g_settings.esp_font_size);
                    break;
                }
            }
        }

        if (!esp_font) {
            esp_font = io.Fonts->AddFontDefault();
            printf("[+] ESP font: ImGui default\n");
        }

        // Store in menu for access
        g_menu.esp_font = esp_font;

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
        if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(h, m, w, l);
    }
};

inline Overlay g_overlay;