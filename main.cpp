#include <Windows.h>
#include <wincodec.h>
#include <cstdio>
#include <chrono>
#include <csignal>
#include <imgui.h>

#include "types.h"
#include "settings.h"
#include "utils.h"
#include "offsets.h"
#include "memory.h"
#include "config.h"
#include "menu.h"
#include "crosshair.h"
#include "overlay.h"
#include "weapon_icons.h"
#include "entity_reader.h"
#include "visible_esp.h"
#include "spectators.h"
#include "radar.h"

static const char* CONFIG_PATH = "cs2esp.ini";
static volatile bool g_running = true;

static void save_and_exit() {
    Config::save(CONFIG_PATH);
    printf("[+] Config saved\n");
}

static BOOL WINAPI console_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT ||
        event == CTRL_BREAK_EVENT || event == CTRL_LOGOFF_EVENT ||
        event == CTRL_SHUTDOWN_EVENT) {
        save_and_exit();
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    SetConsoleCtrlHandler(console_handler, TRUE);
    std::atexit(save_and_exit);

    if (Config::load(CONFIG_PATH))
        printf("[+] Config loaded\n");

    if (!g_memory.attach(L"cs2.exe")) {
        printf("cs2.exe not found\n");
        CoUninitialize();
        return 1;
    }
    printf("[+] Attached (client.dll @ 0x%llX)\n", g_memory.get_client_base());

    if (!g_offsets.load("offsets/offsets.json", "offsets/client_dll.json")) {
        printf("Failed to load offsets\n");
        CoUninitialize();
        return 1;
    }

    if (!g_overlay.init(L"Counter-Strike 2")) {
        printf("Overlay failed\n");
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

        if (GetAsyncKeyState(g_settings.key_menu) & 1) g_menu.toggle();
        if (GetAsyncKeyState(g_settings.key_master) & 1)
            g_settings.master_switch = !g_settings.master_switch;

        if (g_settings.menu_open != prev_menu) {
            g_overlay.set_interactive(g_settings.menu_open);
            prev_menu = g_settings.menu_open;
        }

        if (!g_overlay.begin_frame()) break;
        g_menu.render();

        if (!g_settings.master_switch) {
            g_overlay.end_frame();
            limit_frame(frame_start, g_settings.target_fps);
            continue;
        }

        FrameState state = entity_reader.read_frame(g_overlay.width, g_overlay.height);

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

        g_radar.draw(draw, state.radar_players, EntityList::MAX_PLAYERS,
                     state.local.x, state.local.y, state.local.yaw, state.local.team,
                     g_overlay.width, g_overlay.height);

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

        g_overlay.end_frame();
        limit_frame(frame_start, g_settings.target_fps);
    }

    g_weapon_icons.shutdown();
    g_overlay.shutdown();
    CoUninitialize();
    return 0;
}