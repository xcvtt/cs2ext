#pragma once
#include <imgui.h>
#include <cstring>
#include "settings.h"
#include "types.h"
#include "utils.h"
#include "crosshair.h"
#include "overlay.h"

class Menu {
public:
    void toggle() { g_settings.menu_open = !g_settings.menu_open; }

    void render() {
        if (!g_settings.menu_open) return;

        // Apply saved position on first use
        if (g_settings.menu_x >= 0 && g_settings.menu_y >= 0) {
            ImGui::SetNextWindowPos({g_settings.menu_x, g_settings.menu_y}, ImGuiCond_FirstUseEver);
        }
        ImGui::SetNextWindowSize({440, 700}, ImGuiCond_FirstUseEver);
        ImGui::Begin("CS2 ESP##main", &g_settings.menu_open, ImGuiWindowFlags_NoCollapse);

        // Save menu position every frame
        ImVec2 pos = ImGui::GetWindowPos();
        g_settings.menu_x = pos.x;
        g_settings.menu_y = pos.y;

        if (g_settings.master_switch)
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1}, "ACTIVE (%s to toggle)", vk_name(g_settings.key_master));
        else
            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1}, "DISABLED (%s to toggle)", vk_name(g_settings.key_master));

        if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enabled##esp", &g_settings.esp_enabled);
            ImGui::Checkbox("Box", &g_settings.draw_box);

            if (g_settings.draw_box) {
                ImGui::Indent();
                ImGui::Text("Style:");
                ImGui::RadioButton("Corners##bs", &g_settings.box_style, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Full##bs", &g_settings.box_style, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Dashed##bs", &g_settings.box_style, 2);
                ImGui::SliderFloat("Thickness##bt", &g_settings.box_thickness, 0.5f, 4.0f, "%.1f");
                ImGui::SliderFloat("Padding X", &g_settings.box_padding_x, 0.0f, 20.0f, "%.1f");
                ImGui::SliderFloat("Padding Y", &g_settings.box_padding_y, 0.0f, 20.0f, "%.1f");
                if (g_settings.box_style == 0)
                    ImGui::SliderFloat("Corner Size", &g_settings.box_corner_pct, 0.1f, 0.5f, "%.2f");
                ImGui::Unindent();
            }

            ImGui::Checkbox("Health Bar", &g_settings.draw_healthbar);

            ImGui::Checkbox("Health Text", &g_settings.draw_health_text);
            if (g_settings.draw_health_text) {
                ImGui::Indent();
                if (ImGui::SliderFloat("HP Font Size", &g_settings.hp_font_size, 8.0f, 24.0f, "%.0f"))
                    g_overlay.font_rebuild_needed = true;
                ImGui::ColorEdit4("HP Color##hpc", g_settings.hp_text_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Checkbox("HP Shadow##hps", &g_settings.hp_text_shadow);
                if (g_settings.hp_text_shadow)
                    ImGui::ColorEdit4("HP Shadow##hpsc", g_settings.hp_text_shadow_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Unindent();
            }

            ImGui::Checkbox("Name", &g_settings.draw_name);
            if (g_settings.draw_name) {
                ImGui::Indent();
                ImGui::Text("Position:");
                ImGui::RadioButton("Top##np", &g_settings.name_position, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Bottom##np", &g_settings.name_position, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Left##np", &g_settings.name_position, 2);
                ImGui::SameLine();
                ImGui::RadioButton("Right##np", &g_settings.name_position, 3);
                if (ImGui::SliderFloat("Name Font Size", &g_settings.name_font_size, 8.0f, 24.0f, "%.0f"))
                    g_overlay.font_rebuild_needed = true;
                ImGui::DragFloat("Offset X##no", &g_settings.name_offset_x, 0.5f, -50.0f, 50.0f, "%.1f");
                ImGui::DragFloat("Offset Y##no", &g_settings.name_offset_y, 0.5f, -50.0f, 50.0f, "%.1f");
                ImGui::ColorEdit4("Name Color##nc", g_settings.name_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Checkbox("Name Shadow##ns", &g_settings.name_shadow);
                if (g_settings.name_shadow)
                    ImGui::ColorEdit4("Shadow Color##nsc", g_settings.name_shadow_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Unindent();
            }

            ImGui::Checkbox("Teammates", &g_settings.draw_teammates);
            ImGui::Checkbox("Skeleton Wire", &g_settings.draw_skeleton_wire);
            ImGui::Separator();
            ImGui::Text("Chams:");
            ImGui::RadioButton("Filled", &g_settings.chams_style, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Wire", &g_settings.chams_style, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Glow", &g_settings.chams_style, 2);
            ImGui::SameLine();
            ImGui::RadioButton("Flat", &g_settings.chams_style, 3);
        }

        if (ImGui::CollapsingHeader("Font")) {
            render_font_selector();
        }

        if (ImGui::CollapsingHeader("Crosshair")) {
            ImGui::Checkbox("Enabled##xhair", &g_settings.crosshair_enabled);
            if (g_settings.crosshair_enabled) {
                ImGui::Separator();
                ImGui::Text("Shape:");
                ImGui::RadioButton("+##xs", &g_settings.crosshair_shape, 0);
                ImGui::SameLine();
                ImGui::RadioButton("T##xs", &g_settings.crosshair_shape, 1);
                ImGui::SameLine();
                ImGui::RadioButton("O##xs", &g_settings.crosshair_shape, 2);
                ImGui::SameLine();
                ImGui::RadioButton("Dot##xs", &g_settings.crosshair_shape, 3);
                ImGui::SameLine();
                ImGui::RadioButton("+O##xs", &g_settings.crosshair_shape, 4);
                ImGui::SameLine();
                ImGui::RadioButton("<>##xs", &g_settings.crosshair_shape, 5);
                ImGui::SameLine();
                ImGui::RadioButton(">>##xs", &g_settings.crosshair_shape, 6);

                ImGui::ColorEdit4("Color##xcol", g_settings.crosshair_color,
                                  ImGuiColorEditFlags_NoInputs);
                ImGui::SliderFloat("Size##xsz", &g_settings.crosshair_size, 0.5f, 20, "%.1f");
                ImGui::SliderFloat("Thickness##xth", &g_settings.crosshair_thickness, 0.5f, 5, "%.1f");

                bool has_gap = g_settings.crosshair_shape <= 1 ||
                               g_settings.crosshair_shape == 4 ||
                               g_settings.crosshair_shape == 6;
                if (has_gap)
                    ImGui::SliderFloat("Gap##xgap", &g_settings.crosshair_gap, 0, 10, "%.1f");

                ImGui::Separator();
                ImGui::Checkbox("Outline##xol", &g_settings.crosshair_outline);
                if (g_settings.crosshair_outline) {
                    ImGui::ColorEdit4("Outline Color##xolc", g_settings.crosshair_outline_color,
                                      ImGuiColorEditFlags_NoInputs);
                    ImGui::SliderFloat("Outline Width##xolt",
                                       &g_settings.crosshair_outline_thickness, 1, 3, "%.0f");
                }

                if (g_settings.crosshair_shape != 3) {
                    ImGui::Checkbox("Center Dot##xdot", &g_settings.crosshair_dot);
                    if (g_settings.crosshair_dot)
                        ImGui::SliderFloat("Dot Size##xds",
                                           &g_settings.crosshair_dot_size, 1, 4, "%.0f");
                }

                ImGui::Separator();
                ImGui::Text("Preview:");
                ImVec2 pp = ImGui::GetCursorScreenPos();
                float psz = 60;
                ImGui::InvisibleButton("##xprev", {psz, psz});
                ImDrawList* dl = ImGui::GetWindowDrawList();

                dl->AddRectFilled(pp, {pp.x + psz, pp.y + psz}, IM_COL32(30, 30, 30, 255));
                dl->AddRect(pp, {pp.x + psz, pp.y + psz}, IM_COL32(60, 60, 60, 255));

                dl->PushClipRect(pp, {pp.x + psz, pp.y + psz}, true);
                ImDrawListFlags old = dl->Flags;
                dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
                dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;

                float pcx = floorf(pp.x + psz * 0.5f) + 0.5f;
                float pcy = floorf(pp.y + psz * 0.5f) + 0.5f;

                Crosshair::Config prev_cfg = {
                    true,
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
                g_crosshair.draw_preview(dl, pcx, pcy, prev_cfg);

                dl->Flags = old;
                dl->PopClipRect();
            }
        }

        if (ImGui::CollapsingHeader("Spectator List")) {
            ImGui::Checkbox("Show Spectators", &g_settings.draw_spectators);
        }

        if (ImGui::CollapsingHeader("Radar")) {
            ImGui::Checkbox("Show Radar##r", &g_settings.draw_radar);
            ImGui::Checkbox("Circle Shape##rc", &g_settings.radar_circle);
            ImGui::Checkbox("Rotate with View##rrot", &g_settings.radar_rotate);
            ImGui::Checkbox("Range Rings##rrings", &g_settings.radar_rings);
            ImGui::Checkbox("Names##rn", &g_settings.radar_names);
            ImGui::SliderFloat("Size##rs", &g_settings.radar_size, 100, 400, "%.0f");
            ImGui::SliderFloat("Range##rr", &g_settings.radar_range, 500, 6000, "%.0f");
            ImGui::SliderFloat("Opacity##rop", &g_settings.radar_bg_alpha, 0.1f, 1.0f, "%.2f");
            ImGui::DragFloat("Position X##rx", &g_settings.radar_x, 1, 0, 3000);
            ImGui::DragFloat("Position Y##ry", &g_settings.radar_y, 1, 0, 2000);
        }

        if (ImGui::CollapsingHeader("Colors")) {
            ImGui::Text("Enemy");
            ImGui::ColorEdit4("Fill##ef", g_settings.enemy_fill, ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Outline##eo", g_settings.enemy_outline, ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Glow##eg", g_settings.enemy_glow, ImGuiColorEditFlags_AlphaBar);
            ImGui::Separator();
            ImGui::Text("Team");
            ImGui::ColorEdit4("Fill##tf", g_settings.team_fill, ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Outline##to", g_settings.team_outline, ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Glow##tg", g_settings.team_glow, ImGuiColorEditFlags_AlphaBar);
        }

        if (ImGui::CollapsingHeader("Body Tuning")) {
            ImGui::SliderFloat("Width", &g_settings.body_width_scale, 0.3f, 3.0f);
            ImGui::SliderFloat("Head", &g_settings.head_radius, 1, 10);
            ImGui::SliderFloat("Depth", &g_settings.depth_scale, 100, 1500);
            ImGui::SliderFloat("Glow Out", &g_settings.glow_expand_outer, 0, 15);
            ImGui::SliderFloat("Glow In", &g_settings.glow_expand_inner, 0, 10);
            ImGui::Separator();
            static const char* n[] = {
                "Neck>Sp1", "Sp1>Sp2", "Sp2>Pelv",
                "L Arm", "L FArm", "R Arm", "R FArm",
                "L Thigh", "L Shin", "R Thigh", "R Shin",
                "L SBridge", "R SBridge", "L HBridge", "R HBridge"};
            for (int i = 0; i < 15; i++) {
                ImGui::PushID(i);
                ImGui::Text("%-11s", n[i]);
                ImGui::SameLine(120);
                ImGui::SetNextItemWidth(65);
                ImGui::DragFloat("##a", &g_settings.limb_width_a[i], 0.1f, 0.5f, 15);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(65);
                ImGui::DragFloat("##b", &g_settings.limb_width_b[i], 0.1f, 0.5f, 15);
                ImGui::PopID();
            }
            if (ImGui::Button("Reset Body##body")) {
                float a[] = {6, 7, 6.5f, 3.5f, 3, 3.5f, 3, 4.5f, 3.5f, 4.5f, 3.5f, 3, 3, 5, 5};
                float b[] = {7, 6.5f, 7, 3, 2, 3, 2, 3.5f, 2.5f, 3.5f, 2.5f, 4, 4, 4.5f, 4.5f};
                memcpy(g_settings.limb_width_a, a, sizeof(a));
                memcpy(g_settings.limb_width_b, b, sizeof(b));
                g_settings.body_width_scale = 1.0f;
                g_settings.head_radius = 4.5f;
                g_settings.depth_scale = 500.0f;
            }
        }

        if (ImGui::CollapsingHeader("Key Binds")) {
            render_key_bind("Menu Toggle", g_settings.key_menu, bind_waiting_menu);
            render_key_bind("Master Toggle", g_settings.key_master, bind_waiting_master);
            render_key_bind("Exit", g_settings.key_exit, bind_waiting_exit);
        }

        if (ImGui::CollapsingHeader("Misc", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("FPS", &g_settings.target_fps, 30, 1000, "%.0f");
            ImGui::SliderFloat("Box Smooth", &g_settings.box_smoothing, 0, 0.95f);
            ImGui::Separator();
            ImGui::Text("%s = toggle menu", vk_name(g_settings.key_menu));
            ImGui::Text("%s = master on/off", vk_name(g_settings.key_master));
            ImGui::Text("%s = exit", vk_name(g_settings.key_exit));

            ImGui::Separator();
            if (ImGui::Button("Reset All Settings")) {
                reset_popup_open = true;
            }
        }

        // Reset confirmation popup
        if (reset_popup_open) {
            ImGui::OpenPopup("Reset?##confirm");
            reset_popup_open = false;
        }
        if (ImGui::BeginPopupModal("Reset?##confirm", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Reset ALL settings to defaults?");
            ImGui::Text("This cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Yes, Reset", {120, 0})) {
                g_settings.reset();
                g_overlay.font_rebuild_needed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {120, 0})) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

private:
    bool bind_waiting_menu = false;
    bool bind_waiting_master = false;
    bool bind_waiting_exit = false;
    bool reset_popup_open = false;

    void render_key_bind(const char* label, int& key, bool& waiting) {
        ImGui::Text("%s:", label);
        ImGui::SameLine(140);

        char btn_label[64];
        if (waiting) {
            snprintf(btn_label, sizeof(btn_label), "[Press key...]##%s", label);
        } else {
            snprintf(btn_label, sizeof(btn_label), "%s##%s", vk_name(key), label);
        }

        if (ImGui::Button(btn_label, {120, 0})) {
            waiting = true;
        }

        if (waiting) {
            int pressed = scan_any_key();
            if (pressed > 0) {
                key = pressed;
                waiting = false;
            } else if (pressed == -1) {
                // Escape = cancel
                waiting = false;
            }
        }
    }

    void render_font_selector() {
        auto& fonts = g_overlay.available_fonts;
        if (fonts.empty()) {
            ImGui::Text("No fonts found");
            return;
        }

        const char* preview = (g_settings.esp_font_index >= 0 &&
                               g_settings.esp_font_index < (int)fonts.size())
                                  ? fonts[g_settings.esp_font_index].display_name.c_str()
                                  : "Unknown";

        ImGui::Text("ESP Font Family:");
        if (ImGui::BeginCombo("##fontcombo", preview)) {
            for (int i = 0; i < (int)fonts.size(); i++) {
                bool selected = (g_settings.esp_font_index == i);
                if (ImGui::Selectable(fonts[i].display_name.c_str(), selected)) {
                    if (g_settings.esp_font_index != i) {
                        g_settings.esp_font_index = i;
                        g_overlay.font_rebuild_needed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (g_overlay.esp_font) {
            ImGui::Separator();
            ImGui::Text("Preview:");

            ImGui::PushFont(g_overlay.esp_font);

            float name_sz = g_settings.name_font_size;
            ImVec2 pos_a = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const char* sample = "Player_Name 123";
            ImVec2 ts = g_overlay.esp_font->CalcTextSizeA(name_sz, FLT_MAX, 0, sample);

            dl->AddRectFilled(pos_a, {pos_a.x + ts.x + 8, pos_a.y + ts.y + 4},
                              IM_COL32(20, 20, 20, 200), 3.0f);
            dl->AddText(g_overlay.esp_font, name_sz,
                        {pos_a.x + 4, pos_a.y + 2},
                        float4_to_col(g_settings.name_color), sample);
            ImGui::Dummy({ts.x + 8, ts.y + 6});

            float hp_sz = g_settings.hp_font_size;
            const char* hp_sample = "75 HP";
            ImVec2 pos_b = ImGui::GetCursorScreenPos();
            ImVec2 ts2 = g_overlay.esp_font->CalcTextSizeA(hp_sz, FLT_MAX, 0, hp_sample);
            dl->AddRectFilled(pos_b, {pos_b.x + ts2.x + 8, pos_b.y + ts2.y + 4},
                              IM_COL32(20, 20, 20, 200), 3.0f);
            dl->AddText(g_overlay.esp_font, hp_sz,
                        {pos_b.x + 4, pos_b.y + 2},
                        float4_to_col(g_settings.hp_text_color), hp_sample);
            ImGui::Dummy({ts2.x + 8, ts2.y + 6});

            ImGui::PopFont();
        }

        if (g_overlay.font_rebuild_needed)
            ImGui::TextColored({1, 1, 0, 1}, "Font will rebuild next frame...");
    }
};

inline Menu g_menu;