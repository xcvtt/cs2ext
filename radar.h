#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "types.h"
#include "settings.h"

class Radar {
public:
    void draw(ImDrawList* draw, const RadarPlayer* players, int count,
              float local_x, float local_y, float local_yaw, int local_team,
              int sw, int sh) {
        if (!g_settings.draw_radar || !g_settings.master_switch) return;

        float size = g_settings.radar_size;
        float range = g_settings.radar_range;
        float rx = g_settings.radar_x;
        float ry = g_settings.radar_y;
        float cx = rx + size * 0.5f;
        float cy = ry + size * 0.5f;
        float half = size * 0.5f;
        float alpha = g_settings.radar_bg_alpha;
        ImU32 bg = IM_COL32(15, 15, 15, (int)(alpha * 255));
        ImU32 border = IM_COL32(80, 80, 80, (int)(alpha * 255));

        if (g_settings.radar_circle) {
            draw->AddCircleFilled({cx, cy}, half, bg, 48);
            draw->AddCircle({cx, cy}, half, border, 48, 1.5f);
        } else {
            draw->AddRectFilled({rx, ry}, {rx + size, ry + size}, bg, 4.0f);
            draw->AddRect({rx, ry}, {rx + size, ry + size}, border, 4.0f, 0, 1.5f);
        }

        ImU32 cross_col = IM_COL32(255, 255, 255, (int)(alpha * 50));
        draw->AddLine({cx - 5, cy}, {cx + 5, cy}, cross_col);
        draw->AddLine({cx, cy - 5}, {cx, cy + 5}, cross_col);

        if (g_settings.radar_rings) {
            ImU32 ring_col = IM_COL32(255, 255, 255, (int)(alpha * 25));
            for (float f : {0.25f, 0.5f, 0.75f, 1.0f})
                draw->AddCircle({cx, cy}, half * f, ring_col, 32, 0.5f);
        }

        if (g_settings.radar_circle)
            draw->PushClipRect({rx, ry}, {rx + size, ry + size}, true);

        float yaw_rad = (local_yaw + 90.0f) * 3.14159265f / 180.0f;
        float cos_y = cosf(yaw_rad);
        float sin_y = sinf(yaw_rad);
        float scale = half / range;

        for (int i = 0; i < count; i++) {
            if (!players[i].valid || players[i].health <= 0) continue;

            bool enemy = (players[i].team != local_team);
            if (!enemy && !g_settings.draw_teammates) continue;

            float dx = players[i].x - local_x;
            float dy = players[i].y - local_y;

            float rot_x, rot_y;
            if (g_settings.radar_rotate) {
                rot_x = dx * cos_y + dy * sin_y;
                rot_y = -dx * sin_y + dy * cos_y;
            } else {
                rot_x = dx;
                rot_y = -dy;
            }

            float px = cx + rot_x * scale;
            float py = cy - rot_y * scale;

            float off_x = px - cx, off_y = py - cy;
            float dist_sq = off_x * off_x + off_y * off_y;
            float max_r = half - 4.0f;

            if (g_settings.radar_circle) {
                if (dist_sq > max_r * max_r) {
                    float d = sqrtf(dist_sq);
                    px = cx + off_x / d * max_r;
                    py = cy + off_y / d * max_r;
                }
            } else {
                px = std::clamp(px, rx + 4.0f, rx + size - 4.0f);
                py = std::clamp(py, ry + 4.0f, ry + size - 4.0f);
            }

            ImU32 col = enemy
                            ? float4_to_col(g_settings.enemy_outline)
                            : float4_to_col(g_settings.team_outline);

            float world_dist = sqrtf(dx * dx + dy * dy);
            float dot_r = std::clamp(5.0f - world_dist / range * 2.0f, 2.5f, 5.0f);

            draw->AddCircleFilled({px + 1, py + 1}, dot_r, IM_COL32(0, 0, 0, 80), 12);
            draw->AddCircleFilled({px, py}, dot_r, col, 12);
            draw->AddCircle({px, py}, dot_r, IM_COL32(0, 0, 0, 100), 12, 1.0f);

            if (g_settings.radar_names && players[i].name[0]) {
                ImVec2 ts = ImGui::CalcTextSize(players[i].name);
                float tx = px - ts.x * 0.5f;
                float ty = py - dot_r - ts.y - 1;
                tx = std::clamp(tx, rx + 2, rx + size - ts.x - 2);
                ty = std::clamp(ty, ry + 2, ry + size - ts.y - 2);
                draw->AddText({tx + 1, ty + 1}, IM_COL32(0, 0, 0, 160), players[i].name);
                draw->AddText({tx, ty}, IM_COL32(255, 255, 255, 180), players[i].name);
            }
        }

        float ts = 3.5f;
        if (g_settings.radar_rotate) {
            draw->AddTriangleFilled(
                {cx, cy - ts * 1.3f},
                {cx - ts * 0.8f, cy + ts * 0.5f},
                {cx + ts * 0.8f, cy + ts * 0.5f},
                IM_COL32(255, 255, 255, 220));
        } else {
            float a = (local_yaw + 90.0f) * 3.14159265f / 180.0f;
            float ca = cosf(a), sa = sinf(a);
            auto rot = [&](float lx, float ly) -> ImVec2 {
                return {cx + lx * ca - ly * sa, cy - lx * sa - ly * ca};
            };
            draw->AddTriangleFilled(
                rot(0, ts * 1.3f),
                rot(-ts * 0.8f, -ts * 0.5f),
                rot(ts * 0.8f, -ts * 0.5f),
                IM_COL32(255, 255, 255, 220));
        }

        if (g_settings.radar_circle)
            draw->PopClipRect();
    }
};

inline Radar g_radar;