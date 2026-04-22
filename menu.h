#pragma once
#include <imgui.h>
#include "settings.h"
#include "types.h"
#include "utils.h"
#include "crosshair.h"
#include "grenades.h"
#include "overlay.h"
#include "weapon_icons.h"

class Menu {
public:
    void toggle() { g_settings.menu_open = !g_settings.menu_open; }

    void render() {
        if (!g_settings.menu_open) return;

        if (g_settings.menu_x >= 0 && g_settings.menu_y >= 0)
            ImGui::SetNextWindowPos({g_settings.menu_x, g_settings.menu_y}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({560, 520}, ImGuiCond_FirstUseEver);

        ImGui::PushFont(g_overlay.menu_font);

        ImGui::Begin("##mainwindow", &g_settings.menu_open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImVec2 pos = ImGui::GetWindowPos();
        g_settings.menu_x = pos.x;
        g_settings.menu_y = pos.y;

        ImGui::PushFont(g_overlay.menu_title_font);
        ImVec4 accent = {g_settings.menu_accent_color[0], g_settings.menu_accent_color[1],
                         g_settings.menu_accent_color[2], g_settings.menu_accent_color[3]};
        ImGui::TextColored(accent, "CS2 ESP");
        ImGui::SameLine(ImGui::GetWindowWidth() - 130);
        if (g_settings.master_switch)
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1}, "[ACTIVE]");
        else
            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1}, "[OFF]");
        ImGui::PopFont();

        ImGui::Separator();

        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("Main"))  { render_tab_main();  ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("ESP"))   { render_tab_esp();   ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Radar")) { render_tab_radar(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Aim"))  { render_tab_aim();  ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Misc"))  { render_tab_misc();  ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Nades")) { render_tab_nades(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Menu"))  { render_tab_menu_style(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::PopFont();
    }

private:
    bool bind_waiting_menu = false;
    bool bind_waiting_master = false;
    bool bind_waiting_exit = false;
    bool reset_popup_open = false;
    bool bind_waiting_nade_toggle = false;
    bool bind_waiting_nade_add    = false;
    bool bind_waiting_nade_delete = false;
    bool bind_waiting_aimbot = false;
    bool bind_waiting_trigger = false;

    void render_tab_main() {
        ImGui::Spacing();
        ImGui::Text("Status:");
        ImGui::SameLine();
        if (g_settings.master_switch)
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1}, "Active (%s)", vk_name(g_settings.key_master));
        else
            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1}, "Disabled (%s)", vk_name(g_settings.key_master));

        ImGui::Separator();
        ImGui::Text("Key Binds");
        ImGui::Spacing();
        render_key_bind("Menu Toggle", g_settings.key_menu, bind_waiting_menu);
        render_key_bind("Master Toggle", g_settings.key_master, bind_waiting_master);
        render_key_bind("Exit", g_settings.key_exit, bind_waiting_exit);

        ImGui::Separator();
        ImGui::Text("Performance");
        ImGui::Checkbox("Vsync", &g_settings.use_vsync);

        ImGui::BeginDisabled(g_settings.use_vsync);
        ImGui::SliderFloat("Target FPS", &g_settings.target_fps, 30, 1000, "%.0f");
        ImGui::EndDisabled();
        ImGui::TextColored({0.5f, 0.5f, 0.5f, 1}, "Higher = smoother ESP");

        ImGui::Text("Memory backend");

        static bool backend_changed = false;

        backend_changed |= ImGui::RadioButton("WinApi", &g_settings.memory_backend, 0);
        ImGui::SameLine();
        backend_changed |= ImGui::RadioButton("Syscall", &g_settings.memory_backend, 1);
        ImGui::SameLine();
        backend_changed |= ImGui::RadioButton("Kernel", &g_settings.memory_backend, 2);

        if (backend_changed)
            ImGui::TextColored(ImVec4(1,0.4f,0.2f,1), "Restart required to apply");

        ImGui::Separator();
        if (ImGui::Button("Reset All Settings")) reset_popup_open = true;
        if (reset_popup_open) {
            ImGui::OpenPopup("Reset?##confirm");
            reset_popup_open = false;
        }
        if (ImGui::BeginPopupModal("Reset?##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Reset ALL settings to defaults?");
            ImGui::Separator();
            if (ImGui::Button("Yes", {100, 0})) {
                g_settings.reset();
                g_overlay.font_rebuild_needed = true;
                g_overlay.apply_menu_style();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {100, 0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

// ---- paste this method in place of render_tab_esp() inside class Menu ----
    void render_tab_esp() {
        ImGui::Spacing();
        ImGui::Checkbox("ESP Enabled", &g_settings.esp_enabled);
        ImGui::Separator();

        // ================================================================
        //  ESP THEME
        // ================================================================
        ImVec4 theme_accent = {g_settings.esp_theme_color[0], g_settings.esp_theme_color[1],
                               g_settings.esp_theme_color[2], g_settings.esp_theme_color[3]};
        ImGui::TextColored(theme_accent, "ESP Theme Color");
        ImGui::Checkbox("Use Theme Color##esptheme", &g_settings.esp_use_theme);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "When ON: all ESP element colors (box, name, weapon,\n"
                "healthbar) are automatically derived from the theme\n"
                "color. Enemy gets the raw hue; teammates get a\n"
                "blue-shifted tint. Individual color pickers below\n"
                "are ignored while theme is active.");

        if (g_settings.esp_use_theme) {
            ImGui::Indent();
            ImGui::ColorEdit4("Theme Color##tc", g_settings.esp_theme_color,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            // Quick presets
            ImGui::Text("Presets:");
            struct Preset { const char* name; float r, g, b; };
            static constexpr Preset presets[] = {
                {"Cyan",       0.00f, 0.85f, 1.00f},
                {"Lime",       0.20f, 1.00f, 0.30f},
                {"Blue",       0.235f,0.68f, 0.93f},
                {"Magenta",    0.90f, 0.10f, 0.80f},
                {"White",      0.95f, 0.95f, 0.95f},
                {"Red",        1.00f, 0.15f, 0.15f},
            };
            for (const auto& pr : presets) {
                if (ImGui::Button(pr.name, {65, 0})) {
                    g_settings.esp_theme_color[0] = pr.r;
                    g_settings.esp_theme_color[1] = pr.g;
                    g_settings.esp_theme_color[2] = pr.b;
                    g_settings.esp_theme_color[3] = 1.0f;
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::Unindent();

            // While theme is active, gray out individual color pickers with a note
            ImGui::TextColored({0.5f, 0.5f, 0.5f, 1}, "(Individual colors below are overridden by theme)");
            ImGui::Separator();
        } else {
            ImGui::Separator();
        }

        // ================================================================
        //  CHAMS / SKELETON
        // ================================================================
        ImGui::Text("Chams Style:");
        ImGui::RadioButton("Filled",   &g_settings.chams_style, 0); ImGui::SameLine();
        ImGui::RadioButton("Wire",     &g_settings.chams_style, 1); ImGui::SameLine();
        ImGui::RadioButton("Glow",     &g_settings.chams_style, 2); ImGui::SameLine();
        ImGui::RadioButton("Skeleton", &g_settings.chams_style, 3);

        ImGui::Checkbox("Draw Head", &g_settings.draw_head);
        ImGui::Checkbox("Teammates", &g_settings.draw_teammates);
        ImGui::Separator();

        // ================================================================
        // DISTANCE OPACITY
        // ================================================================
        ImGui::Checkbox("Distance Opacity Drop", &g_settings.esp_opacity_drop);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Fade out ESP elements with distance.\nDoes not affect radar or spectator list.");
        if (g_settings.esp_opacity_drop) {
            ImGui::Indent();
            ImGui::SliderFloat("Fade Start##opd", &g_settings.esp_opacity_drop_start,
                               100.0f, 3000.0f, "%.0f units");
            ImGui::SliderFloat("Fade End##opd",   &g_settings.esp_opacity_drop_end,
                               200.0f, 5000.0f, "%.0f units");
            ImGui::SliderFloat("Min Opacity##opd", &g_settings.esp_opacity_drop_min,
                               0.0f, 1.0f, "%.2f");
            // Clamp: start must be less than end
            if (g_settings.esp_opacity_drop_start >= g_settings.esp_opacity_drop_end)
                g_settings.esp_opacity_drop_start = g_settings.esp_opacity_drop_end - 100.0f;
            ImGui::Unindent();
        }
        ImGui::Separator();

        // ================================================================
        //  BOX
        // ================================================================
        ImGui::Checkbox("Box", &g_settings.draw_box);
        if (g_settings.draw_box) {
            ImGui::Indent();
            ImGui::RadioButton("Corners##bs", &g_settings.box_style, 0); ImGui::SameLine();
            ImGui::RadioButton("Full##bs",    &g_settings.box_style, 1); ImGui::SameLine();
            ImGui::RadioButton("Dashed##bs",  &g_settings.box_style, 2);
            ImGui::SliderFloat("Thickness##bt", &g_settings.box_thickness,  0.5f, 4.0f, "%.1f");
            ImGui::SliderFloat("Padding X",     &g_settings.box_padding_x,  0.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("Padding Y",     &g_settings.box_padding_y,  0.0f, 20.0f, "%.1f");
            if (g_settings.box_style == 0)
                ImGui::SliderFloat("Corner %", &g_settings.box_corner_pct, 0.1f, 0.5f, "%.2f");
            ImGui::Unindent();
        }
        ImGui::Separator();

        // ================================================================
        //  HEALTH BAR
        // ================================================================
        ImGui::Checkbox("Health Bar", &g_settings.draw_healthbar);
        if (g_settings.draw_healthbar) {
            ImGui::Indent();

            // Solid color toggle (grayed out when theme controls it)
            if (g_settings.esp_use_theme) {
                ImGui::BeginDisabled();
                bool forced = true;
                ImGui::Checkbox("Solid Color (theme override)##hbsc", &forced);
                ImGui::EndDisabled();
            } else {
                ImGui::Checkbox("Solid Color##hbsc", &g_settings.healthbar_solid_color);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "ON: flat single color (set below)\n"
                        "OFF: classic green / yellow / red gradient");
            }

            if (g_settings.healthbar_solid_color && !g_settings.esp_use_theme) {
                ImGui::ColorEdit4("Bar Color##hbcol", g_settings.healthbar_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }
            ImGui::Unindent();
        }

        ImGui::Checkbox("Health Text", &g_settings.draw_health_text);
        if (g_settings.draw_health_text) {
            ImGui::Indent();
            if (ImGui::SliderFloat("HP Font##hpf", &g_settings.hp_font_size, 8, 24, "%.0f"))
                g_overlay.font_rebuild_needed = true;
            if (!g_settings.esp_use_theme) {
                ImGui::ColorEdit4("HP Color##hpc", g_settings.hp_text_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }
            ImGui::Checkbox("HP Shadow", &g_settings.hp_text_shadow);
            if (g_settings.hp_text_shadow)
                ImGui::ColorEdit4("Shadow##hps", g_settings.hp_text_shadow_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::Unindent();
        }
        ImGui::Separator();

        // ================================================================
        //  NAME
        // ================================================================
        ImGui::Checkbox("Name", &g_settings.draw_name);
        if (g_settings.draw_name) {
            ImGui::Indent();
            ImGui::RadioButton("Top##np",   &g_settings.name_position, 0); ImGui::SameLine();
            ImGui::RadioButton("Bot##np",   &g_settings.name_position, 1);
            if (ImGui::SliderFloat("Name Font##nf", &g_settings.name_font_size, 8, 24, "%.0f"))
                g_overlay.font_rebuild_needed = true;
            ImGui::DragFloat("Offset X##no", &g_settings.name_offset_x, 0.5f, -50, 50, "%.1f");
            ImGui::DragFloat("Offset Y##no", &g_settings.name_offset_y, 0.5f, -50, 50, "%.1f");
            if (!g_settings.esp_use_theme) {
                ImGui::ColorEdit4("Name##nc", g_settings.name_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }
            ImGui::Checkbox("Shadow##ns", &g_settings.name_shadow);
            if (g_settings.name_shadow)
                ImGui::ColorEdit4("Shadow##nsc", g_settings.name_shadow_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::Unindent();
        }
        ImGui::Separator();

        // ================================================================
        //  WEAPON
        // ================================================================
        ImGui::Checkbox("Weapon", &g_settings.draw_weapon);
        if (g_settings.draw_weapon) {
            ImGui::Indent();
            ImGui::Checkbox("Show Icon##wi", &g_settings.weapon_show_icon);
            if (g_settings.weapon_show_icon && !g_weapon_icons.has_any_icons()) {
                ImGui::SameLine();
                ImGui::TextColored({1, 0.6f, 0.2f, 1}, "(no icons in icons/)");
            }
            ImGui::Checkbox("Show Text##wt", &g_settings.weapon_show_text);
            if (ImGui::SliderFloat("Weapon Font##wf", &g_settings.weapon_font_size, 8, 20, "%.0f"))
                g_overlay.font_rebuild_needed = true;
            ImGui::SliderFloat("Dist. Dropoff##wdd", &g_settings.weapon_distance_dropoff, 0, 1, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0 = constant size\n1 = scales with distance\n0.3 = default");
            if (!g_settings.esp_use_theme) {
                ImGui::ColorEdit4("Text Color##wc", g_settings.weapon_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::ColorEdit4("Icon Tint##wic", g_settings.weapon_icon_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            }
            ImGui::Checkbox("Weapon Shadow##ws", &g_settings.weapon_shadow);
            if (g_settings.weapon_shadow)
                ImGui::ColorEdit4("Shadow##wsc", g_settings.weapon_shadow_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::Unindent();
        }
        ImGui::Separator();

        // ================================================================
        //  ESP FONT
        // ================================================================
        ImGui::Text("ESP Font:");
        render_esp_font_selector();
        ImGui::Separator();

        // ================================================================
        //  BODY TUNING
        // ================================================================
        ImGui::Text("Body Tuning");
        ImGui::SliderFloat("Body Width",  &g_settings.body_width_scale, 0.3f, 3.0f,  "%.2f");
        ImGui::SliderFloat("Head Size",   &g_settings.head_radius,      1,    10,     "%.1f");
        ImGui::SliderFloat("Depth Scale", &g_settings.depth_scale,      100,  1500,   "%.0f");
        ImGui::SliderFloat("Glow Outer",  &g_settings.glow_expand_outer, 0,   15,     "%.1f");
        ImGui::SliderFloat("Glow Inner",  &g_settings.glow_expand_inner, 0,   10,     "%.1f");
        if (ImGui::Button("Reset Body")) {
            g_settings.body_width_scale  = 1.0f;
            g_settings.head_radius       = 6.0f;
            g_settings.depth_scale       = 500.0f;
            g_settings.glow_expand_outer = 6.0f;
            g_settings.glow_expand_inner = 3.0f;
        }
        ImGui::Separator();

        // ================================================================
        //  ESP COLORS  (only shown when theme is OFF)
        // ================================================================
        if (!g_settings.esp_use_theme) {
            ImGui::Text("ESP Colors");
            ImGui::Columns(2, nullptr, false);
            ImGui::Text("Enemy");
            ImGui::ColorEdit4("Fill##ef",    g_settings.enemy_fill,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Outline##eo", g_settings.enemy_outline, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Glow##eg",    g_settings.enemy_glow,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::NextColumn();
            ImGui::Text("Team");
            ImGui::ColorEdit4("Fill##tf",    g_settings.team_fill,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Outline##to", g_settings.team_outline, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Glow##tg",    g_settings.team_glow,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
            ImGui::Columns(1);
        }
    }

    void render_tab_radar() {
        ImGui::Spacing();
        ImGui::Checkbox("Show Radar", &g_settings.draw_radar);
        ImGui::Separator();
        ImGui::Checkbox("Circle Shape", &g_settings.radar_circle);
        ImGui::Checkbox("Rotate with View", &g_settings.radar_rotate);
        ImGui::Checkbox("Range Rings", &g_settings.radar_rings);
        ImGui::Checkbox("Player Names", &g_settings.radar_names);
        if (g_settings.radar_names) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Names Font##rnf", &g_settings.radar_names_font_size, 8.0f, 20.0f, "%.0f")) {
                g_overlay.font_rebuild_needed = true;
            }
            ImGui::Unindent();
        }
        ImGui::Separator();
        ImGui::SliderFloat("Size", &g_settings.radar_size, 100, 1000, "%.0f");
        ImGui::SliderFloat("Zoom", &g_settings.radar_zoom, 0.1f, 1.0f, "%.2fx");
        ImGui::SliderFloat("Opacity", &g_settings.radar_bg_alpha, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Position X", &g_settings.radar_x, 1, 0, 3000);
        ImGui::DragFloat("Position Y", &g_settings.radar_y, 1, 0, 2000);

        ImGui::Separator();
        ImGui::Text("Radar Colors");
        ImGui::ColorEdit4("Enemy##re", g_settings.radar_enemy_color,
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Team##rt", g_settings.radar_team_color,
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
    }

    void render_tab_aim() {
        ImGui::TextColored({1.0f,0.0f,0.0f,1}, "Aim assist can get you VACLIVE banned.");
        ImGui::Checkbox("Enable aimbot", &g_settings.aimbot_enabled);

        if (g_settings.aimbot_enabled) {
            render_key_bind("Key",  g_settings.key_aimbot, bind_waiting_aimbot);
            ImGui::SliderInt("Fov", &g_settings.aimbot_fov, 1, 360);
            ImGui::SliderFloat("Smoothing", &g_settings.aimbot_smooth, 1.0f, 20.0f, "%.1f");
            ImGui::Checkbox("Head only", &g_settings.aimbot_head_only);
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Enable triggerbot", &g_settings.triggerbot_enabled);

        if (g_settings.triggerbot_enabled)
        {
            render_key_bind("Trigger key", g_settings.key_triggerbot, bind_waiting_trigger);
            ImGui::SliderInt("Delay ms", &g_settings.triggerbot_delay, 0, 300);
            ImGui::Checkbox("Head only##trigger", &g_settings.triggerbot_head_only);
        }

        ImGui::Separator();
    }

    void render_tab_misc() {
        ImGui::Spacing();

        ImGui::Text("Spectator List");
        ImGui::Checkbox("Show Spectators", &g_settings.draw_spectators);
        if (g_settings.draw_spectators) {
            ImGui::Indent();
            ImGui::DragFloat("Spec X##sx", &g_settings.spec_x, 1, -1, 3000, "%.0f");
            ImGui::SameLine();
            if (ImGui::Button("Auto##specauto")) g_settings.spec_x = -1.0f;
            ImGui::DragFloat("Spec Y##sy", &g_settings.spec_y, 1, 0, 2000, "%.0f");
            ImGui::Unindent();
        }

        ImGui::Separator();

        ImGui::Text("Crosshair");
        ImGui::Checkbox("Enabled##xhair", &g_settings.crosshair_enabled);
        if (g_settings.crosshair_enabled) {
            ImGui::Indent();
            ImGui::Text("Shape:");
            ImGui::RadioButton("+##xs", &g_settings.crosshair_shape, 0); ImGui::SameLine();
            ImGui::RadioButton("T##xs", &g_settings.crosshair_shape, 1); ImGui::SameLine();
            ImGui::RadioButton("O##xs", &g_settings.crosshair_shape, 2); ImGui::SameLine();
            ImGui::RadioButton("Dot##xs", &g_settings.crosshair_shape, 3); ImGui::SameLine();
            ImGui::RadioButton("+O##xs", &g_settings.crosshair_shape, 4); ImGui::SameLine();

            ImGui::ColorEdit4("Color##xcol", g_settings.crosshair_color, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Size##xsz", &g_settings.crosshair_size, 0.5f, 20, "%.1f");
            ImGui::SliderFloat("Thick##xth", &g_settings.crosshair_thickness, 0.5f, 5, "%.1f");

            bool has_gap = g_settings.crosshair_shape <= 1 ||
                           g_settings.crosshair_shape == 4 ||
                           g_settings.crosshair_shape == 6;
            if (has_gap)
                ImGui::SliderFloat("Gap##xgap", &g_settings.crosshair_gap, -10, 10, "%.1f");

            ImGui::Checkbox("Outline##xol", &g_settings.crosshair_outline);
            if (g_settings.crosshair_outline) {
                ImGui::ColorEdit4("OL Color##xolc", g_settings.crosshair_outline_color, ImGuiColorEditFlags_NoInputs);
                ImGui::SliderFloat("OL Width##xolt", &g_settings.crosshair_outline_thickness, 1, 3, "%.0f");
            }
            if (g_settings.crosshair_shape != 3) {
                ImGui::Checkbox("Center Dot##xdot", &g_settings.crosshair_dot);
                if (g_settings.crosshair_dot)
                    ImGui::SliderFloat("Dot Size##xds", &g_settings.crosshair_dot_size, 1, 4, "%.0f");
            }

            ImGui::Separator();
            ImVec2 pp = ImGui::GetCursorScreenPos();
            float psz = 60;
            ImGui::InvisibleButton("##xprev", {psz, psz});
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pp, {pp.x + psz, pp.y + psz}, IM_COL32(20, 20, 20, 255));
            dl->AddRect(pp, {pp.x + psz, pp.y + psz}, IM_COL32(50, 50, 50, 255));
            dl->PushClipRect(pp, {pp.x + psz, pp.y + psz}, true);
            ImDrawListFlags old = dl->Flags;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            Crosshair::Config prev_cfg = {
                true, g_settings.crosshair_shape, g_settings.crosshair_size,
                g_settings.crosshair_gap, g_settings.crosshair_thickness,
                float4_to_col(g_settings.crosshair_color),
                g_settings.crosshair_outline, g_settings.crosshair_outline_thickness,
                float4_to_col(g_settings.crosshair_outline_color),
                g_settings.crosshair_dot, g_settings.crosshair_dot_size,
            };
            float pcx = floorf(pp.x + psz * 0.5f) + 0.5f;
            float pcy = floorf(pp.y + psz * 0.5f) + 0.5f;
            g_crosshair.draw_preview(dl, pcx, pcy, prev_cfg);
            dl->Flags = old;
            dl->PopClipRect();
            ImGui::Unindent();
        }
    }

        void render_tab_nades() {
        ImGui::Spacing();
        ImGui::Checkbox("Grenade Helper", &g_settings.grenade_helper_enabled);

        if (!g_settings.grenade_helper_enabled) {
            ImGui::TextColored({0.5f,0.5f,0.5f,1},
                "Enable to configure and use grenade lineups.");
            return;
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 130);
        if (g_settings.grenade_helper_visible)
            ImGui::TextColored({0.3f,1.0f,0.3f,1}, "[VISIBLE]");
        else
            ImGui::TextColored({0.6f,0.6f,0.6f,1}, "[HIDDEN]");

        ImGui::Separator();

        // Key binds
        ImGui::Text("Key Binds");
        render_key_bind("Toggle Visible",  g_settings.key_grenade_toggle, bind_waiting_nade_toggle);
        render_key_bind("Add Spot",        g_settings.key_grenade_add,    bind_waiting_nade_add);
        render_key_bind("Delete Spot",     g_settings.key_grenade_delete, bind_waiting_nade_delete);
        ImGui::Separator();

        // Filters
        ImGui::Text("Filters");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f,0.70f,0.70f,1));
        ImGui::Checkbox("Smoke##nf",   &g_settings.grenade_filter_smoke);
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f,0.47f,0.12f,1));
        ImGui::Checkbox("Molotov##nf", &g_settings.grenade_filter_molotov);
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f,0.86f,0.31f,1));
        ImGui::Checkbox("Frag##nf",    &g_settings.grenade_filter_frag);
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f,1.00f,0.39f,1));
        ImGui::Checkbox("Flash##nf",   &g_settings.grenade_filter_flash);
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Appearance
        ImGui::Text("Appearance");
        ImGui::SliderFloat("Circle Radius##nc",    &g_settings.grenade_circle_radius,    10.0f, 150.0f, "%.0f");
        ImGui::SliderFloat("Circle Thickness##nc", &g_settings.grenade_circle_thickness,  0.5f,   4.0f, "%.1f");
        if (ImGui::SliderFloat("Text Size##nc",        &g_settings.grenade_text_font_size,    8.0f,  20.0f, "%.0f")) {
            g_overlay.font_rebuild_needed = true;
        }
        ImGui::Spacing();
        ImGui::ColorEdit4("Circle##ncc",        g_settings.grenade_circle_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::ColorEdit4("Active Circle##nac", g_settings.grenade_circle_active_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::ColorEdit4("Aim Line##nal",      g_settings.grenade_aim_line_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::ColorEdit4("Text##ntc",          g_settings.grenade_text_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::Separator();

        // Spot list for current map
        ImGui::Text("Spots");
        ImGui::BeginChild("##nadespots", {0, 200}, true);
        g_grenades.render_spot_list();
        ImGui::EndChild();
    }

    void render_tab_menu_style() {
        ImGui::Spacing();
        ImGui::Text("Menu Appearance");
        ImGui::Separator();

        bool style_changed = false;
        style_changed |= ImGui::ColorEdit4("Accent Color", g_settings.menu_accent_color,
                                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        style_changed |= ImGui::ColorEdit4("Border Color", g_settings.menu_border_color,
                                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        style_changed |= ImGui::SliderFloat("BG Opacity", &g_settings.menu_bg_alpha, 0.3f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Presets:");
        if (ImGui::Button("Cyber Green", {120, 0})) {
            float cc[] = {0, 1, 0.65f, 1}; float b[] = {0, 1, 0.65f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Blood Red", {120, 0})) {
            float cc[] = {1, 0.2f, 0.15f, 1}; float b[] = {1, 0.2f, 0.15f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Electric Blue", {120, 0})) {
            float cc[] = {0.2f, 0.5f, 1, 1}; float b[] = {0.2f, 0.5f, 1, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        if (ImGui::Button("Purple Haze", {120, 0})) {
            float cc[] = {0.7f, 0.3f, 1, 1}; float b[] = {0.7f, 0.3f, 1, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.90f; style_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Gold", {120, 0})) {
            float cc[] = {1, 0.8f, 0.2f, 1}; float b[] = {1, 0.8f, 0.2f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Minimal", {120, 0})) {
            float cc[] = {0.7f, 0.7f, 0.7f, 1}; float b[] = {0.4f, 0.4f, 0.4f, 0.2f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.88f; style_changed = true;
        }

        if (style_changed)
            g_overlay.apply_menu_style();

        ImGui::Separator();

        ImGui::Text("Menu Font");
        render_menu_font_selector();

        if (ImGui::SliderFloat("Menu Font Size", &g_settings.menu_font_size, 10.0f, 22.0f, "%.0f"))
            g_overlay.font_rebuild_needed = true;
    }

    void render_key_bind(const char* label, int& key, bool& waiting, bool allow_mouse1 = false) {
        ImGui::Text("%s:", label);
        ImGui::SameLine(160);

        char btn[64];
        if (waiting)
            snprintf(btn, sizeof(btn), "[...]##%s", label);
        else
            snprintf(btn, sizeof(btn), "%s##%s", vk_name(key), label);

        if (ImGui::Button(btn, {100, 0})) {
            waiting = true;
        }

        if (waiting) {
            int pressed = scan_any_key(allow_mouse1);
            if (pressed > 0)   { key = pressed; waiting = false; }
            else if (pressed == -1) { waiting = false; } // Escape = cancel
        }
    }

    void render_esp_font_selector() {
        auto& fonts = g_overlay.available_fonts;
        if (fonts.empty()) { ImGui::Text("No fonts"); return; }
        const char* preview = (g_settings.esp_font_index >= 0 &&
                               g_settings.esp_font_index < (int)fonts.size())
                                  ? fonts[g_settings.esp_font_index].display_name.c_str() : "?";
        if (ImGui::BeginCombo("##espfont", preview)) {
            for (int i = 0; i < (int)fonts.size(); i++) {
                bool sel = (g_settings.esp_font_index == i);
                if (ImGui::Selectable(fonts[i].display_name.c_str(), sel)) {
                    if (g_settings.esp_font_index != i) {
                        g_settings.esp_font_index = i;
                        g_overlay.font_rebuild_needed = true;
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (g_overlay.esp_font) {
            ImGui::PushFont(g_overlay.esp_font);
            float sz = g_settings.name_font_size;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const char* sample = "Player_Name 123";
            ImVec2 ts = g_overlay.esp_font->CalcTextSizeA(sz, FLT_MAX, 0, sample);
            dl->AddRectFilled(p, {p.x + ts.x + 8, p.y + ts.y + 4}, IM_COL32(15, 15, 15, 200), 3);
            dl->AddText(g_overlay.esp_font, sz, {p.x + 4, p.y + 2},
                        float4_to_col(g_settings.name_color), sample);
            ImGui::Dummy({ts.x + 8, ts.y + 6});
            ImGui::PopFont();
        }
    }

    void render_menu_font_selector() {
        auto& fonts = g_overlay.menu_fonts;
        if (fonts.empty()) { ImGui::Text("No fonts"); return; }
        const char* preview = (g_settings.menu_font_index >= 0 &&
                               g_settings.menu_font_index < (int)fonts.size())
                                  ? fonts[g_settings.menu_font_index].display_name.c_str() : "?";
        if (ImGui::BeginCombo("##menufont", preview)) {
            for (int i = 0; i < (int)fonts.size(); i++) {
                bool sel = (g_settings.menu_font_index == i);
                if (ImGui::Selectable(fonts[i].display_name.c_str(), sel)) {
                    if (g_settings.menu_font_index != i) {
                        g_settings.menu_font_index = i;
                        g_overlay.font_rebuild_needed = true;
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
};

inline Menu g_menu;