#include <Windows.h>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <imgui.h>

#include "offsets.h"
#include "memory.h"
#include "config.h"
#include "menu.h"
#include "crosshair.h"
#include "overlay.h"
#include "visible_esp.h"
#include "spectators.h"
#include "radar.h"

static const char* CONFIG_PATH = "cs2esp.ini";

int main() {
    if (Config::load(CONFIG_PATH))
        printf("[+] Config loaded\n");

    if (!g_offsets.load("offsets/offsets.json", "offsets/client_dll.json")) {
        printf("Failed to load offsets\n"); return 1;
    }
    if (!g_memory.attach(L"cs2.exe")) {
        printf("cs2.exe not found\n"); return 1;
    }
    printf("[+] Attached (client.dll @ 0x%llX)\n", g_memory.client_base);

    if (!g_overlay.init(L"Counter-Strike 2")) {
        printf("Overlay failed\n"); return 1;
    }
    printf("[+] F1 = menu | F2 = master toggle | INSERT = exit\n");

    CBoneData bone_buf[MAX_BONE];
    PlayerVisuals players[64];
    RadarPlayer radar_players[64];
    bool prev_menu = g_settings.menu_open;
    int spec_tick = 0;

    while (!(GetAsyncKeyState(VK_INSERT) & 1)) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        if (GetAsyncKeyState(VK_F1) & 1) g_menu.toggle();
        if (GetAsyncKeyState(VK_F2) & 1) g_settings.master_switch = !g_settings.master_switch;

        if (g_settings.menu_open != prev_menu) {
            g_overlay.update_clickthrough(g_settings.menu_open);
            prev_menu = g_settings.menu_open;
        }

        if (!g_overlay.begin_frame()) break;
        g_menu.render();

        if (!g_settings.master_switch) {
            g_overlay.end_frame();
            limit_frame(frame_start, g_settings.target_fps);
            continue;
        }

        Matrix4x4 view_matrix;
        g_memory.read_raw(g_memory.client_base + g_offsets.client.dwViewMatrix,
            &view_matrix, 64);

        uintptr_t local_pawn = g_memory.read<uintptr_t>(
            g_memory.client_base + g_offsets.client.dwLocalPlayerPawn);
        uintptr_t local_controller = g_memory.read<uintptr_t>(
            g_memory.client_base + g_offsets.client.dwLocalPlayerController);

        int local_team = 0;
        float local_x = 0, local_y = 0, local_yaw = 0;
        bool is_scoped = false;

        if (local_pawn) {
            local_team = g_memory.read<int>(local_pawn + g_offsets.C_BaseEntity.m_iTeamNum);
            is_scoped = g_memory.read<bool>(local_pawn + g_offsets.C_CSPlayerPawn.m_bIsScoped);

            uintptr_t local_scene = g_memory.read<uintptr_t>(
                local_pawn + g_offsets.C_BaseEntity.m_pGameSceneNode);
            if (local_scene) {
                Vec3 origin = g_memory.read<Vec3>(
                    local_scene + g_offsets.CGameSceneNode.m_vecAbsOrigin);
                local_x = origin.x;
                local_y = origin.y;
            }
            local_yaw = atan2f(view_matrix.m[0][1], view_matrix.m[0][0])
                        * 180.0f / 3.14159265f - 90.0f;
        }

        uintptr_t entity_list = g_memory.read<uintptr_t>(
            g_memory.client_base + g_offsets.client.dwEntityList);

        for (int i = 1; i < 64; i++) {
            players[i].valid = false;
            radar_players[i].valid = false;
        }

        if (entity_list) {
            if (++spec_tick >= 15) {
                spec_tick = 0;
                g_spectators.update(entity_list, local_pawn, local_controller);
            }

            uintptr_t list_entry = g_memory.read<uintptr_t>(entity_list + 16);
            if (list_entry) {
                for (int i = 1; i < 64; i++) {
                    uintptr_t controller = g_memory.read<uintptr_t>(
                        list_entry + 112 * (i & 0x1FF));
                    if (!controller) continue;

                    char name[128]{};
                    uintptr_t name_ptr = g_memory.read<uintptr_t>(
                        controller + g_offsets.CCSPlayerController.m_sSanitizedPlayerName);
                    if (name_ptr)
                        g_memory.read_raw(name_ptr, name, 127);
                    name[127] = 0;
                    for (int c = 0; name[c]; c++)
                        if ((unsigned char)name[c] < 0x20) name[c] = ' ';

                    uint32_t pawn_handle = g_memory.read<uint32_t>(
                        controller + g_offsets.CCSPlayerController.m_hPawn);
                    if (!pawn_handle)
                        pawn_handle = g_memory.read<uint32_t>(
                            controller + g_offsets.CCSPlayerController.m_hPlayerPawn);
                    if (!pawn_handle) continue;

                    uintptr_t pawn_entry = g_memory.read<uintptr_t>(
                        entity_list + 8 * ((pawn_handle & 0x7FFF) >> 9) + 16);
                    if (!pawn_entry) continue;

                    uintptr_t pawn = g_memory.read<uintptr_t>(
                        pawn_entry + 112 * (pawn_handle & 0x1FF));
                    if (!pawn || pawn == local_pawn) continue;

                    int health = g_memory.read<int>(pawn + g_offsets.C_BaseEntity.m_iHealth);
                    if (health <= 0) {
                        g_esp.smoothed_boxes[i].initialized = false;
                        continue;
                    }

                    int team = g_memory.read<int>(pawn + g_offsets.C_BaseEntity.m_iTeamNum);

                    uintptr_t scene_node = g_memory.read<uintptr_t>(
                        pawn + g_offsets.C_BaseEntity.m_pGameSceneNode);
                    if (!scene_node) continue;

                    Vec3 origin = g_memory.read<Vec3>(
                        scene_node + g_offsets.CGameSceneNode.m_vecAbsOrigin);

                    uintptr_t bone_array = g_memory.read<uintptr_t>(
                        scene_node + g_offsets.CSkeletonInstance.m_modelState + 0x80);
                    if (!bone_array) continue;
                    if (!g_memory.read_raw(bone_array, bone_buf, sizeof(bone_buf)))
                        continue;

                    players[i].valid = true;
                    players[i].team = team;
                    players[i].health = health;
                    players[i].origin = origin;
                    memcpy(players[i].name, name, 128);

                    for (int b = 0; b < MAX_BONE; b++)
                        players[i].visible[b] = w2s_depth(
                            bone_buf[b].pos, view_matrix,
                            g_overlay.width, g_overlay.height,
                            players[i].screens[b], players[i].depths[b]);

                    radar_players[i] = {origin.x, origin.y, origin.z,
                                        team, health, true, {}};
                    memcpy(radar_players[i].name, name, 128);
                }
            }
        }

        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (int i = 1; i < 64; i++) {
            if (!players[i].valid)
                g_esp.smoothed_boxes[i].initialized = false;
            g_esp.draw_player(draw, players[i], local_team,
                g_overlay.width, g_overlay.height, i, is_scoped);
        }

        g_radar.draw(draw, radar_players, 64,
            local_x, local_y, local_yaw, local_team,
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

    // Auto-save on exit
    Config::save(CONFIG_PATH);
    printf("[+] Config saved\n");

    g_overlay.shutdown();
    return 0;
}