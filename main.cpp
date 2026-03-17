#include <Windows.h>
#include <cstdio>
#include <chrono>
#include <imgui.h>

#include "types.h"
#include "settings.h"
#include "utils.h"
#include "offsets.h"
#include "memory/imemory.h"
#include "memory/memory_winapi.h"
#include "memory/memory_syscall.h"
#include "memory/memory_driver.h"
#include "memory/driver_manager.h"
#include "config.h"
#include "menu.h"
#include "crosshair.h"
#include "overlay.h"
#include "weapon_icons.h"
#include "entity_reader.h"
#include "visible_esp.h"
#include "spectators.h"
#include "radar.h"
#include "grenades.h"

static const char* CONFIG_PATH = "cs2esp.ini";
static volatile bool g_running = true;
static bool g_driver_backend_active = false;

static void save_and_exit() {
    Config::save(CONFIG_PATH);
    printf("[+] Config saved\n");
}

// Cleanup that ALWAYS runs, no matter how we exit
static void cleanup_on_exit() {
    // Close memory backend (triggers driver unload if driver backend)
    if (g_memory) {
        g_memory->close();
        g_memory.reset();
    }

    save_and_exit();
}

static BOOL WINAPI console_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT ||
        event == CTRL_BREAK_EVENT || event == CTRL_LOGOFF_EVENT ||
        event == CTRL_SHUTDOWN_EVENT)
    {
        cleanup_on_exit();
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

// Crash handler to ensure driver cleanup even on crash
static LONG WINAPI crash_handler(EXCEPTION_POINTERS* ex) {
    UNREFERENCED_PARAMETER(ex);
    printf("\n[!] Crash detected, cleaning up driver...\n");

    // Force driver cleanup directly (g_memory might be in bad state)
    if (g_driver_backend_active) {
        DriverManager::full_cleanup();
    }

    return EXCEPTION_CONTINUE_SEARCH; // Let Windows handle the crash
}

enum MemoryBackend {
    WinApi = 0,
    IndirectSyscall = 1,
    KernelDriver = 2,
};

std::unique_ptr<IMemory> CreateMemoryBackend(MemoryBackend backend) {
    switch (backend) {
    case WinApi:          return std::make_unique<MemoryWinApi>();
    case IndirectSyscall: return std::make_unique<MemorySyscall>();
    case KernelDriver:    return std::make_unique<MemoryDriver>();
    default:              throw std::runtime_error("invalid backend");
    }
}

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();

    // Set up all exit handlers FIRST
    SetConsoleCtrlHandler(console_handler, TRUE);
    SetUnhandledExceptionFilter(crash_handler);
    std::atexit([]() {
        if (g_driver_backend_active) {
            DriverManager::full_cleanup();
        }
    });

    if (Config::load(CONFIG_PATH))
        printf("[+] Config loaded\n");

    g_grenades.init("grenades.json");

    while (g_settings.memory_backend == -1) {
        printf("\nChoose memory reading backend:\n");
        printf("  0. User-space (WinAPI)              - simplest, works everywhere\n");
        printf("  1. User-space (indirect syscalls)    - slightly stealthier\n");
        printf("  2. Kernel-space driver (IOCTL)       - requires admin + setup\n");
        printf("\n> ");

        int backend = -1;
        scanf_s("%d", &backend);
        if (backend < 0 || backend > 2) {
            printf("Invalid choice: %d\n", backend);
            continue;
        }

        // If kernel driver, warn user about requirements
        if (backend == 2) {
            printf("\n");
            printf("=== Kernel Driver Requirements ===\n");
            printf("  - Run as Administrator\n");
            printf("  - Secure Boot DISABLED in BIOS\n");
            printf("  - Test signing will be enabled (requires reboot on first use)\n");
            printf("  - 'Test Mode' watermark will appear on desktop\n");
            printf("  - MemReader.sys must be next to this .exe\n");
            printf("\n  The driver will be automatically loaded and unloaded.\n");
            printf("  No permanent changes besides test signing.\n");
            printf("\n  Continue? (y/n): ");

            char c;
            scanf_s(" %c", &c, 1);
            if (c != 'y' && c != 'Y') {
                printf("Cancelled. Choose another backend.\n");
                continue;
            }
        }

        g_settings.memory_backend = backend;
    }

    // Track if we're using driver backend (for cleanup handlers)
    g_driver_backend_active = (g_settings.memory_backend == KernelDriver);

    // Create and attach
    try {
        g_memory = CreateMemoryBackend(
            static_cast<MemoryBackend>(g_settings.memory_backend));
    }
    catch (const std::exception& e) {
        printf("[-] Failed to create backend: %s\n", e.what());
        CoUninitialize();
        return 1;
    }

    if (!g_memory->attach(L"cs2.exe")) {
        printf("[-] Failed to attach to cs2.exe.\n");

        if (g_driver_backend_active) {
            printf("[*] Cleaning up driver...\n");
            g_memory->close();
            g_memory.reset();
            g_driver_backend_active = false;
        }

        CoUninitialize();
        return 1;
    }

    printf("[+] Attached to cs2.exe (PID: %lu)\n", g_memory->get_pid());
    printf("[+] client.dll base: 0x%llX\n",
        (unsigned long long)g_memory->get_client_base());

    if (g_driver_backend_active) {
        printf("[+] Using KERNEL DRIVER for memory reads\n");
    }

    if (!g_offsets.load("offsets/offsets.json", "offsets/client_dll.json")) {
        printf("[-] Failed to load offsets\n");
        cleanup_on_exit();
        CoUninitialize();
        return 1;
    }

    if (!g_overlay.init(L"Counter-Strike 2")) {
        printf("[-] Overlay failed\n");
        cleanup_on_exit();
        CoUninitialize();
        return 1;
    }

    g_weapon_icons.init(g_overlay.get_device());

    printf("[+] %s = menu | %s = master toggle | %s = exit\n",
           vk_name(g_settings.key_menu),
           vk_name(g_settings.key_master),
           vk_name(g_settings.key_exit));

    EntityReader entity_reader;
    bool prev_menu = g_settings.menu_open;
    int spec_tick = 0;

    g_overlay.set_interactive(g_settings.menu_open);

    while (g_running) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        if (GetAsyncKeyState(g_settings.key_exit) & 1) break;

        if ((GetAsyncKeyState(g_settings.key_menu) & 1) && g_overlay.is_game_window()) g_menu.toggle();
        if (GetAsyncKeyState(g_settings.key_master) & 1)
            g_settings.master_switch = !g_settings.master_switch;

        if (g_settings.menu_open != prev_menu) {
            g_overlay.set_interactive(g_settings.menu_open);
            prev_menu = g_settings.menu_open;
        }

        if (!g_overlay.begin_frame()) break;
        g_menu.render();

        if (!g_settings.master_switch) {
            static constexpr int IDLE_FPS_CAP = 20;
            g_overlay.end_frame(0);
            limit_frame(frame_start, IDLE_FPS_CAP);
            continue;
        }

        FrameState state = entity_reader.read_frame(
            g_overlay.width, g_overlay.height);

        float fwd_x = state.view_matrix.m[2][0];
        float fwd_y = state.view_matrix.m[2][1];
        float fwd_z = state.view_matrix.m[2][2];

        float view_pitch_deg = -asinf(std::clamp(fwd_z, -1.0f, 1.0f))
                                * 180.0f / 3.14159265f;
        float view_yaw_deg   =  atan2f(fwd_y, fwd_x)
                                * 180.0f / 3.14159265f;

        g_grenades.set_view_matrix(state.view_matrix);
        g_grenades.update(state.local.x, state.local.y, state.local.z,
                          view_pitch_deg, view_yaw_deg, state.map_name);

        if (state.entity_list) {
            if (++spec_tick >= 15) {
                spec_tick = 0;
                g_spectators.update(state.entity_list,
                    state.local.pawn, state.local.controller);
            }
        }

        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (int i = 1; i < EntityList::MAX_PLAYERS; i++) {
            g_esp.draw_player(draw, state.players[i], state.local.team,
                g_overlay.width, g_overlay.height, i, state.local.is_scoped);
        }

        g_grenades.render_popups();
        g_grenades.draw(draw,
                        state.local.x, state.local.y, state.local.z,
                        g_overlay.width, g_overlay.height);

        g_radar.draw(draw, state.radar_players, EntityList::MAX_PLAYERS,
            state.local.x, state.local.y, state.local.yaw, state.local.team,
            state.map_scale, g_overlay.width, g_overlay.height);

        g_spectators.draw(g_overlay.width);

        ImDrawList* fg = ImGui::GetForegroundDrawList();
        Crosshair::Config xhair_cfg = {
            g_settings.crosshair_enabled && g_settings.master_switch,
            g_settings.crosshair_shape,
            g_settings.crosshair_size,
            g_settings.crosshair_gap,
            g_settings.crosshair_thickness,
            float4_to_col(g_settings.crosshair_color),
            g_settings.crosshair_outline,
            g_settings.crosshair_outline_thickness,
            float4_to_col(g_settings.crosshair_outline_color),
            g_settings.crosshair_dot,
            g_settings.crosshair_dot_size,
        };
        g_crosshair.draw(fg, g_overlay.width, g_overlay.height, xhair_cfg);

        g_overlay.end_frame(g_settings.use_vsync ? 1 : 0);

        if (!g_settings.use_vsync) {
            limit_frame(frame_start, g_settings.target_fps);
        }
    }

    // Clean exit
    printf("\n[*] Shutting down...\n");
    g_weapon_icons.shutdown();
    g_overlay.shutdown();
    cleanup_on_exit();
    CoUninitialize();
    return 0;
}