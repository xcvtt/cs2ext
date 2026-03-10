#pragma once
#include <Windows.h>

struct MenuSettings {
    bool master_switch = true;
    bool esp_enabled = true;
    bool draw_box = true;
    bool draw_healthbar = true;
    bool draw_health_text = true;
    bool draw_name = true;
    bool draw_teammates = false;
    bool draw_skeleton_wire = false;
    bool draw_spectators = true;
    bool draw_radar = true;
    int chams_style = 2;

    float target_fps = 240.0f;

    float enemy_fill[4]    = {0.86f, 0.16f, 0.16f, 0.47f};
    float enemy_outline[4] = {1.00f, 0.24f, 0.24f, 0.86f};
    float enemy_glow[4]    = {1.00f, 0.20f, 0.20f, 0.10f};
    float team_fill[4]     = {0.16f, 0.39f, 0.86f, 0.47f};
    float team_outline[4]  = {0.24f, 0.51f, 1.00f, 0.86f};
    float team_glow[4]     = {0.20f, 0.39f, 1.00f, 0.10f};

    float body_width_scale = 1.0f;
    float head_radius = 4.5f;
    float depth_scale = 500.0f;
    float glow_expand_outer = 6.0f;
    float glow_expand_inner = 3.0f;
    float box_smoothing = 0.5f;
    float limb_width_a[15] = {6, 7, 6.5f, 3.5f, 3, 3.5f, 3, 4.5f, 3.5f, 4.5f, 3.5f, 3, 3, 5, 5};
    float limb_width_b[15] = {7, 6.5f, 7, 3, 2, 3, 2, 3.5f, 2.5f, 3.5f, 2.5f, 4, 4, 4.5f, 4.5f};

    bool menu_open = true;

    float radar_size = 200.0f;
    float radar_range = 2500.0f;
    float radar_x = 10.0f;
    float radar_y = 10.0f;
    bool radar_rotate = true;
    bool radar_circle = true;
    bool radar_rings = true;
    bool radar_names = false;
    float radar_bg_alpha = 0.85f;

    int box_style = 0;
    float box_thickness = 1.5f;
    float box_padding_x = 5.0f;
    float box_padding_y = 5.0f;
    float box_corner_pct = 0.2f;

    // Font
    int esp_font_index = 0;
    float esp_font_atlas_size = 20.0f;

    // Name ESP
    int name_position = 0;
    float name_offset_x = 0.0f;
    float name_offset_y = -3.0f;
    float name_color[4] = {1.0f, 1.0f, 1.0f, 0.94f};
    float name_shadow_color[4] = {0.0f, 0.0f, 0.0f, 0.78f};
    bool name_shadow = true;
    float name_font_size = 13.0f;

    // Health text
    float hp_text_color[4] = {1.0f, 1.0f, 1.0f, 0.86f};
    float hp_text_shadow_color[4] = {0.0f, 0.0f, 0.0f, 0.70f};
    bool hp_text_shadow = true;
    float hp_font_size = 13.0f;

    // Crosshair
    bool crosshair_enabled = false;
    int crosshair_shape = 0;
    float crosshair_size = 5.0f;
    float crosshair_gap = 2.0f;
    float crosshair_thickness = 1.5f;
    float crosshair_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    bool crosshair_outline = false;
    float crosshair_outline_thickness = 1.0f;
    float crosshair_outline_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    bool crosshair_dot = false;
    float crosshair_dot_size = 1.5f;

    // Menu window position
    float menu_x = -1.0f;  // -1 = auto/default
    float menu_y = -1.0f;

    // Key binds
    int key_menu = VK_F1;
    int key_master = VK_F2;
    int key_exit = VK_INSERT;

    // Reset to defaults
    void reset() { *this = MenuSettings{}; }
};

inline MenuSettings g_settings;