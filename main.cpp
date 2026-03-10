#include <Windows.h>
#include <cstdio>
#include <chrono>
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
#include "entity_reader.h"
#include "visible_esp.h"
#include "spectators.h"
#include "radar.h"

static const char* CONFIG_PATH = "cs2esp.ini";

int main() {
    if (Config::load(CONFIG_PATH))
        printf("[+] Config loaded\n");

    if (!g_offsets.load("offsets/offsets.json", "offsets/client_dll.json")) {
        printf("Failed to load offsets\n");
        return 1;
    }
    if (!g_memory.attach(L"cs2.exe")) {
        printf("cs2.exe not found\n");
        return 1;
    }
    printf("[+] Attached (client.dll @ 0x%llX)\n", g_memory.get_client_base());

    if (!g_overlay.init(L"Counter-Strike 2")) {
        printf("Overlay failed\n");
        return 1;
    }
    printf("[+] F1 = menu | F2 = master toggle | INSERT = exit\n");

    EntityReader entity_reader;
    bool prev_menu = g_settings.menu_open;
    int spec_tick = 0;

    while (!(GetAsyncKeyState(VK_INSERT) & 1)) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        // Input
        if (GetAsyncKeyState(VK_F1) & 1) g_menu.toggle();
        if (GetAsyncKeyState(VK_F2) & 1) g_settings.master_switch = !g_settings.master_switch;

        if (g_settings.menu_open != prev_menu) {
            g_overlay.update_clickthrough(g_settings.menu_open);
            prev_menu = g_settings.menu_open;
        }

        // Begin frame
        if (!g_overlay.begin_frame()) break;
        g_menu.render();

        if (!g_settings.master_switch) {
            g_overlay.end_frame();
            limit_frame(frame_start, g_settings.target_fps);
            continue;
        }

        // Read game state
        FrameState state = entity_reader.read_frame(g_overlay.width, g_overlay.height);

        // Spectator update (throttled)
        if (state.entity_list) {
            if (++spec_tick >= 15) {
                spec_tick = 0;
                g_spectators.update(state.entity_list,
                                    state.local.pawn, state.local.controller);
            }
        }

        // Draw ESP
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (int i = 1; i < EntityList::MAX_PLAYERS; i++) {
            if (!state.players[i].valid)
                g_esp.invalidate_box(i);
            g_esp.draw_player(draw, state.players[i], state.local.team,
                              g_overlay.width, g_overlay.height, i, state.local.is_scoped);
        }

        // Draw radar
        g_radar.draw(draw, state.radar_players, EntityList::MAX_PLAYERS,
                     state.local.x, state.local.y, state.local.yaw, state.local.team,
                     g_overlay.width, g_overlay.height);

        // Draw spectators
        g_spectators.draw(g_overlay.width);

        // Draw crosshair
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

    Config::save(CONFIG_PATH);
    printf("[+] Config saved\n");

    g_overlay.shutdown();
    return 0;
}