#pragma once
#include <imgui.h>
#include <chrono>
#include <thread>

#include "crosshair.h"

enum class ChamsStyle;

static void limit_frame(std::chrono::high_resolution_clock::time_point frame_start,
                        double target_fps = 60.0) {
    using namespace std::chrono;
    nanoseconds target(static_cast<long long>(1'000'000'000.0 / target_fps));
    nanoseconds remaining = target - (high_resolution_clock::now() - frame_start);
    if (remaining > nanoseconds::zero()) {
        nanoseconds sleep_time = remaining - milliseconds(2);
        if (sleep_time > nanoseconds::zero())
            std::this_thread::sleep_for(sleep_time);
        while (high_resolution_clock::now() - frame_start < target) {}
    }
}

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
    float esp_font_size = 13.0f;

    float enemy_fill[4]    = {0.86f,0.16f,0.16f,0.47f};
    float enemy_outline[4] = {1.00f,0.24f,0.24f,0.86f};
    float enemy_glow[4]    = {1.00f,0.20f,0.20f,0.10f};
    float team_fill[4]     = {0.16f,0.39f,0.86f,0.47f};
    float team_outline[4]  = {0.24f,0.51f,1.00f,0.86f};
    float team_glow[4]     = {0.20f,0.39f,1.00f,0.10f};

    float body_width_scale = 1.0f;
    float head_radius = 4.5f;
    float depth_scale = 500.0f;
    float glow_expand_outer = 6.0f;
    float glow_expand_inner = 3.0f;
    float box_smoothing = 0.5f;
    float limb_width_a[15] = {6,7,6.5f,3.5f,3,3.5f,3,4.5f,3.5f,4.5f,3.5f,3,3,5,5};
    float limb_width_b[15] = {7,6.5f,7,3,2,3,2,3.5f,2.5f,3.5f,2.5f,4,4,4.5f,4.5f};

    bool menu_open = true;

    // Radar section in MenuSettings:
    float radar_size = 200.0f;
    float radar_range = 2500.0f;
    float radar_x = 10.0f;
    float radar_y = 10.0f;
    bool radar_rotate = true;
    bool radar_circle = true;
    bool radar_rings = true;
    bool radar_names = false;
    float radar_bg_alpha = 0.85f;

    // Box
    int box_style = 0;
    float box_thickness = 1.5f;
    float box_padding_x = 5.0f;
    float box_padding_y = 5.0f;
    float box_corner_pct = 0.2f;

    // Crosshair
    bool crosshair_enabled = false;
    int crosshair_shape = 0;        // 0=cross 1=tcross 2=circle 3=dot 4=cross+circle 5=diamond 6=arrows
    float crosshair_size = 5.0f;
    float crosshair_gap = 2.0f;
    float crosshair_thickness = 1.5f;
    float crosshair_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    bool crosshair_outline = false;  // off by default
    float crosshair_outline_thickness = 1.0f;
    float crosshair_outline_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    bool crosshair_dot = false;
    float crosshair_dot_size = 1.5f;
};

inline MenuSettings g_settings;

inline ImU32 float4_to_col(const float c[4]) {
    return IM_COL32((int)(c[0]*255),(int)(c[1]*255),(int)(c[2]*255),(int)(c[3]*255));
}

static const char* vk_name(int vk) {
    switch (vk) {
        case VK_LBUTTON: return "Mouse1";
        case VK_RBUTTON: return "Mouse2";
        case VK_MBUTTON: return "Mouse3";
        case VK_XBUTTON1: return "Mouse4";
        case VK_XBUTTON2: return "Mouse5";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        case VK_CAPITAL: return "CapsLock";
        case VK_TAB: return "Tab";
        case VK_SPACE: return "Space";
        default: {
            static char buf[32];
            UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            if (GetKeyNameTextA(scan << 16, buf, 32)) return buf;
            snprintf(buf, 32, "Key 0x%02X", vk);
            return buf;
        }
    }
}

class Menu {
public:
    ImFont* esp_font = nullptr;
    bool rebuild_font = false;

    void toggle() { g_settings.menu_open = !g_settings.menu_open; }

    void render() {
        if (!g_settings.menu_open) return;

        ImGui::SetNextWindowSize({440, 660}, ImGuiCond_FirstUseEver);
        ImGui::Begin("CS2 ESP##main", &g_settings.menu_open, ImGuiWindowFlags_NoCollapse);

        if (g_settings.master_switch)
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1}, "ACTIVE (F2 to toggle)");
        else
            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1}, "DISABLED (F2 to toggle)");

        if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enabled##esp", &g_settings.esp_enabled);
            ImGui::Checkbox("Box", &g_settings.draw_box);

            if (g_settings.draw_box) {
                ImGui::Indent();
                ImGui::Text("Style:");
                ImGui::RadioButton("Corners##bs", &g_settings.box_style, 0); ImGui::SameLine();
                ImGui::RadioButton("Full##bs", &g_settings.box_style, 1); ImGui::SameLine();
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
            ImGui::Checkbox("Name", &g_settings.draw_name);
            ImGui::Checkbox("Teammates", &g_settings.draw_teammates);
            ImGui::Checkbox("Skeleton Wire", &g_settings.draw_skeleton_wire);
            ImGui::Separator();
            ImGui::Text("Chams:");
            ImGui::RadioButton("Filled", &g_settings.chams_style, 0); ImGui::SameLine();
            ImGui::RadioButton("Wire", &g_settings.chams_style, 1); ImGui::SameLine();
            ImGui::RadioButton("Glow", &g_settings.chams_style, 2); ImGui::SameLine();
            ImGui::RadioButton("Flat", &g_settings.chams_style, 3);

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

                // Preview
                ImGui::Separator();
                ImGui::Text("Preview:");
                ImVec2 pp = ImGui::GetCursorScreenPos();
                float psz = 60;
                ImGui::InvisibleButton("##xprev", {psz, psz});
                ImDrawList* dl = ImGui::GetWindowDrawList();

                dl->AddRectFilled(pp, {pp.x+psz, pp.y+psz}, IM_COL32(30,30,30,255));
                dl->AddRect(pp, {pp.x+psz, pp.y+psz}, IM_COL32(60,60,60,255));

                dl->PushClipRect(pp, {pp.x+psz, pp.y+psz}, true);
                ImDrawListFlags old = dl->Flags;
                dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
                dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;

                float pcx = floorf(pp.x + psz*0.5f) + 0.5f;
                float pcy = floorf(pp.y + psz*0.5f) + 0.5f;

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
                extern Crosshair g_crosshair;
                g_crosshair.draw_preview(dl, pcx, pcy, prev_cfg);

                dl->Flags = old;
                dl->PopClipRect();
            }
        }

        if (ImGui::CollapsingHeader("Font / Text")) {
            if (ImGui::SliderFloat("ESP Font Size", &g_settings.esp_font_size, 8.0f, 24.0f, "%.0f"))
                rebuild_font = true;
            if (rebuild_font)
                ImGui::TextColored({1, 1, 0, 1}, "Restart to apply font size");
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
            ImGui::DragFloat("X##rx", &g_settings.radar_x, 1, 0, 3000);
            ImGui::DragFloat("Y##ry", &g_settings.radar_y, 1, 0, 2000);
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
                "Neck>Sp1","Sp1>Sp2","Sp2>Pelv",
                "L Arm","L FArm","R Arm","R FArm",
                "L Thigh","L Shin","R Thigh","R Shin",
                "L SBridge","R SBridge","L HBridge","R HBridge"
            };
            for (int i = 0; i < 15; i++) {
                ImGui::PushID(i);
                ImGui::Text("%-11s", n[i]); ImGui::SameLine(120);
                ImGui::SetNextItemWidth(65);
                ImGui::DragFloat("##a", &g_settings.limb_width_a[i], 0.1f, 0.5f, 15);
                ImGui::SameLine(); ImGui::SetNextItemWidth(65);
                ImGui::DragFloat("##b", &g_settings.limb_width_b[i], 0.1f, 0.5f, 15);
                ImGui::PopID();
            }
            if (ImGui::Button("Reset##body")) {
                float a[]={6,7,6.5f,3.5f,3,3.5f,3,4.5f,3.5f,4.5f,3.5f,3,3,5,5};
                float b[]={7,6.5f,7,3,2,3,2,3.5f,2.5f,3.5f,2.5f,4,4,4.5f,4.5f};
                memcpy(g_settings.limb_width_a, a, sizeof(a));
                memcpy(g_settings.limb_width_b, b, sizeof(b));
                g_settings.body_width_scale = 1.0f;
                g_settings.head_radius = 4.5f;
                g_settings.depth_scale = 500.0f;
            }
        }

        if (ImGui::CollapsingHeader("Misc", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("FPS", &g_settings.target_fps, 30, 1000, "%.0f");
            ImGui::SliderFloat("Box Smooth", &g_settings.box_smoothing, 0, 0.95f);
            ImGui::Separator();
            ImGui::Text("F1 = toggle menu");
            ImGui::Text("F2 = master on/off");
            ImGui::Text("INSERT = exit");
        }

        ImGui::End();
    }
};

inline Menu g_menu;