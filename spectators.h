#pragma once
#include <imgui.h>
#include <vector>
#include <string>
#include <cstdint>

#include "memory.h"
#include "offsets.h"

struct MenuSettings;
extern MenuSettings g_settings;

class Overlay;
extern Overlay g_overlay;

class SpectatorList {
public:
    std::vector<std::string> spectators;

    void update(uintptr_t entity_list, uintptr_t local_pawn, uintptr_t local_controller) {
        spectators.clear();
        if (!entity_list || !local_pawn) return;

        uint32_t local_handle_player = 0;
        uint32_t local_handle_pawn = 0;
        if (local_controller) {
            local_handle_player = g_memory.read<uint32_t>(
                local_controller + g_offsets.CCSPlayerController.m_hPlayerPawn);
            local_handle_pawn = g_memory.read<uint32_t>(
                local_controller + g_offsets.CCSPlayerController.m_hPawn);
        }

        for (int page = 0; page < 8; page++) {
            uintptr_t list_entry = g_memory.read<uintptr_t>(entity_list + 8 * page + 16);
            if (!list_entry) continue;

            for (int idx = 0; idx < 512; idx++) {
                uintptr_t controller = g_memory.read<uintptr_t>(list_entry + 112 * idx);
                if (!controller || controller == local_controller) continue;

                char name[128]{};
                uintptr_t name_ptr = g_memory.read<uintptr_t>(
                    controller + g_offsets.CCSPlayerController.m_sSanitizedPlayerName);
                if (name_ptr)
                    g_memory.read_raw(name_ptr, name, 127);
                name[127] = 0;
                if (!name[0]) continue;
                for (int c = 0; name[c]; c++)
                    if ((unsigned char)name[c] < 0x20) name[c] = ' ';

                uint32_t pawn_handle = g_memory.read<uint32_t>(
                    controller + g_offsets.CCSPlayerController.m_hPawn);
                if (!pawn_handle)
                    pawn_handle = g_memory.read<uint32_t>(
                        controller + g_offsets.CCSPlayerController.m_hPlayerPawn);
                if (!pawn_handle) continue;

                uint32_t entry_idx = (pawn_handle & 0x7FFF) >> 9;
                uintptr_t pawn_entry = g_memory.read<uintptr_t>(
                    entity_list + 8 * entry_idx + 16);
                if (!pawn_entry) continue;

                uintptr_t pawn = g_memory.read<uintptr_t>(
                    pawn_entry + 112 * (pawn_handle & 0x1FF));
                if (!pawn || pawn == local_pawn) continue;

                int health = g_memory.read<int>(pawn + g_offsets.C_BaseEntity.m_iHealth);
                int team = g_memory.read<int>(pawn + g_offsets.C_BaseEntity.m_iTeamNum);
                if (health > 0 && team != 1) continue;

                uintptr_t obs_svc = g_memory.read<uintptr_t>(
                    pawn + g_offsets.C_BasePlayerPawn.m_pObserverServices);
                if (!obs_svc) continue;

                uint32_t obs_target = g_memory.read<uint32_t>(
                    obs_svc + g_offsets.CPlayer_ObserverServices.m_hObserverTarget);
                if (!obs_target || obs_target == 0xFFFFFFFF) continue;

                bool match = false;
                if (local_handle_pawn && obs_target == local_handle_pawn)
                    match = true;
                if (!match && local_handle_player && obs_target == local_handle_player)
                    match = true;
                if (!match) {
                    uint32_t oe = (obs_target & 0x7FFF) >> 9;
                    uintptr_t oel = g_memory.read<uintptr_t>(entity_list + 8 * oe + 16);
                    if (oel) {
                        uintptr_t op = g_memory.read<uintptr_t>(oel + 112 * (obs_target & 0x1FF));
                        if (op == local_pawn) match = true;
                    }
                }

                if (match) {
                    bool dup = false;
                    for (const auto& s : spectators)
                        if (s == name) { dup = true; break; }
                    if (!dup)
                        spectators.push_back(name);
                }
            }
        }
    }

    void draw(int screen_w) {
        if (!g_settings.draw_spectators || !g_settings.master_switch) return;

        char title[64];
        snprintf(title, sizeof(title), "Spectators (%d)###spec", (int)spectators.size());

        float window_w = 280.0f;

        ImGui::SetNextWindowPos(
            {(float)screen_w - window_w - 10.0f, 10.0f},
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({window_w, 0}, {window_w, 1000});
        ImGui::SetNextWindowBgAlpha(0.7f);

        int flags = ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoInputs;

        // Push UTF-8 font for entire window
        ImFont* font = g_overlay.esp_font;
        if (font) ImGui::PushFont(font);

        ImGui::Begin(title, nullptr, flags);

        for (const auto& s : spectators)
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1}, "> %s", s.c_str());

        ImGui::End();

        if (font) ImGui::PopFont();
    }
};

inline SpectatorList g_spectators;