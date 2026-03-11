#pragma once
#include <imgui.h>
#include "types.h"
#include "settings.h"
#include "chams_renderer.h"
#include "box_renderer.h"
#include "overlay.h"

class VisibleESP {
public:
    BoxRenderer box_renderer;

    void draw_player(ImDrawList* draw, const PlayerVisuals& p, int local_team,
                     int sw, int sh, int idx, bool is_scoped) {
        if (!p.valid || !g_settings.esp_enabled || !g_settings.master_switch) return;
        bool enemy = (p.team != local_team);
        if (!enemy && !g_settings.draw_teammates) return;

        // --- Theme: overwrite per-element colors from single accent ---
        if (g_settings.esp_use_theme) {
            EspTheme::apply(enemy);
            // Solid healthbar color is also theme-driven when theme is on
            g_settings.healthbar_solid_color = true;
        }

        ColorSet c = enemy ? get_enemy_colors() : get_team_colors();
        ChamsStyle style = static_cast<ChamsStyle>(g_settings.chams_style);

        float depth_scale = g_settings.depth_scale * (is_scoped ? 2.0f : 1.0f);

        ChamsRenderer::draw_chams(draw, p, c, style, depth_scale);

        if (g_settings.draw_box || g_settings.draw_healthbar ||
            g_settings.draw_health_text || g_settings.draw_name ||
            g_settings.draw_weapon) {
            box_renderer.draw_box_hp_name(draw, p, c, idx, is_scoped,
                                          g_overlay.esp_font,
                                          g_settings.esp_font_atlas_size);
            }
    }
};

inline VisibleESP g_esp;