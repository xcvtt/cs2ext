#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "types.h"
#include "settings.h"
#include "chams_renderer.h"

enum class BoxStyle { CORNERS, FULL, DASHED };
enum class NamePosition { TOP = 0, BOTTOM, LEFT, RIGHT };

struct SmoothedBox {
    float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool initialized = false;
};

class BoxRenderer {
public:
    SmoothedBox smoothed_boxes[64];

    void reset_smoothing() {
        for (auto& b : smoothed_boxes)
            b.initialized = false;
    }

    void draw_box_hp_name(ImDrawList* d, const PlayerVisuals& p,
                          const ColorSet& c, int idx, bool is_scoped,
                          ImFont* font, float font_size) {
        float rmin_x = 1e9f, rmin_y = 1e9f, rmax_x = -1e9f, rmax_y = -1e9f;
        float avg_depth = 0;
        int cnt = 0;
        for (int b : BOX_STABLE_BONES) {
            if (!p.visible[b]) continue;
            rmin_x = std::min(rmin_x, p.screens[b].x);
            rmin_y = std::min(rmin_y, p.screens[b].y);
            rmax_x = std::max(rmax_x, p.screens[b].x);
            rmax_y = std::max(rmax_y, p.screens[b].y);
            avg_depth += p.depths[b];
            cnt++;
        }
        if (cnt < 3) return;
        avg_depth /= cnt;

        float ds = g_settings.depth_scale;
        if (is_scoped) ds *= 2.0f;
        float pad_x = g_settings.box_padding_x * ds / avg_depth;
        float pad_y = g_settings.box_padding_y * ds / avg_depth;
        rmin_x -= pad_x;
        rmax_x += pad_x;
        rmin_y -= pad_y * 0.6f;
        rmax_y += pad_y * 0.4f;

        auto& sb = smoothed_boxes[idx];
        float s = g_settings.box_smoothing;
        if (!sb.initialized) {
            sb = {rmin_x, rmin_y, rmax_x, rmax_y, true};
        } else {
            sb.min_x = sb.min_x * s + rmin_x * (1 - s);
            sb.min_y = sb.min_y * s + rmin_y * (1 - s);
            sb.max_x = sb.max_x * s + rmax_x * (1 - s);
            sb.max_y = sb.max_y * s + rmax_y * (1 - s);
            sb.min_x = std::min(sb.min_x, rmin_x);
            sb.min_y = std::min(sb.min_y, rmin_y);
            sb.max_x = std::max(sb.max_x, rmax_x);
            sb.max_y = std::max(sb.max_y, rmax_y);
        }

        float x0 = sb.min_x, y0 = sb.min_y, x1 = sb.max_x, y1 = sb.max_y;
        float box_thick = g_settings.box_thickness;

        if (g_settings.draw_box)
            draw_box(d, x0, y0, x1, y1, box_thick, c);

        if (g_settings.draw_healthbar)
            draw_healthbar(d, x0, y0, x1, y1, p.health, font,
                           g_settings.hp_font_size);

        if (g_settings.draw_name && p.name[0] && font)
            draw_name(d, x0, y0, x1, y1, p.name, font, avg_depth);
    }

private:
    void draw_box(ImDrawList* d, float x0, float y0, float x1, float y1,
                  float box_thick, const ColorSet& c) {
        BoxStyle style = static_cast<BoxStyle>(g_settings.box_style);
        ImU32 bg = IM_COL32(0, 0, 0, 80);

        switch (style) {
        case BoxStyle::CORNERS: {
            float w = x1 - x0, h = y1 - y0;
            float corner = std::min(w, h) * g_settings.box_corner_pct;
            auto corners = [&](ImU32 col, float t) {
                d->AddLine({x0, y0}, {x0 + corner, y0}, col, t);
                d->AddLine({x0, y0}, {x0, y0 + corner}, col, t);
                d->AddLine({x1, y0}, {x1 - corner, y0}, col, t);
                d->AddLine({x1, y0}, {x1, y0 + corner}, col, t);
                d->AddLine({x0, y1}, {x0 + corner, y1}, col, t);
                d->AddLine({x0, y1}, {x0, y1 - corner}, col, t);
                d->AddLine({x1, y1}, {x1 - corner, y1}, col, t);
                d->AddLine({x1, y1}, {x1, y1 - corner}, col, t);
            };
            corners(bg, box_thick + 2);
            corners(c.outline, box_thick);
            break;
        }
        case BoxStyle::FULL:
            d->AddRect({x0 - 1, y0 - 1}, {x1 + 1, y1 + 1}, bg, 0, 0, box_thick + 2);
            d->AddRect({x0, y0}, {x1, y1}, c.outline, 0, 0, box_thick);
            break;
        case BoxStyle::DASHED: {
            float dash = 8.0f, gap = 5.0f;
            auto dashed_line = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
                float dx = b.x - a.x, dy = b.y - a.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len < 1) return;
                float nx = dx / len, ny = dy / len;
                float pos = 0;
                while (pos < len) {
                    float end = std::min(pos + dash, len);
                    d->AddLine({a.x + nx * pos, a.y + ny * pos},
                               {a.x + nx * end, a.y + ny * end}, col, thick);
                    pos = end + gap;
                }
            };
            dashed_line({x0, y0}, {x1, y0}, bg, box_thick + 2);
            dashed_line({x1, y0}, {x1, y1}, bg, box_thick + 2);
            dashed_line({x1, y1}, {x0, y1}, bg, box_thick + 2);
            dashed_line({x0, y1}, {x0, y0}, bg, box_thick + 2);
            dashed_line({x0, y0}, {x1, y0}, c.outline, box_thick);
            dashed_line({x1, y0}, {x1, y1}, c.outline, box_thick);
            dashed_line({x1, y1}, {x0, y1}, c.outline, box_thick);
            dashed_line({x0, y1}, {x0, y0}, c.outline, box_thick);
            break;
        }
        }
    }

    void draw_healthbar(ImDrawList* d, float x0, float y0, float x1, float y1,
                        int health, ImFont* font, float hp_font_size) {
        float bw = 3, bx = x0 - bw - 4, bh = y1 - y0;
        float hp = std::clamp(health / 100.0f, 0.0f, 1.0f);
        float filled = bh * hp;
        d->AddRectFilled({bx - 1, y0 - 1}, {bx + bw + 1, y1 + 1}, IM_COL32(0, 0, 0, 140));
        d->AddRectFilled({bx, y0}, {bx + bw, y1}, IM_COL32(30, 30, 30, 180));
        uint8_t r = (uint8_t)(255 * (1 - hp)), g = (uint8_t)(255 * hp);
        d->AddRectFilled({bx, y0 + (bh - filled)}, {bx + bw, y1}, IM_COL32(r, g, 0, 230));

        if (g_settings.draw_health_text && health < 100 && font) {
            char txt[8];
            snprintf(txt, 8, "%d", health);
            ImVec2 ts = font->CalcTextSizeA(hp_font_size, FLT_MAX, 0, txt);
            float tx = bx - ts.x - 2;
            float ty = y0 + (bh - filled) - ts.y * 0.5f;

            ImU32 shadow_col = float4_to_col(g_settings.hp_text_shadow_color);
            ImU32 text_col = float4_to_col(g_settings.hp_text_color);

            if (g_settings.hp_text_shadow)
                d->AddText(font, hp_font_size, {tx + 1, ty + 1}, shadow_col, txt);
            d->AddText(font, hp_font_size, {tx, ty}, text_col, txt);
        }
    }

    void draw_name(ImDrawList* d, float x0, float y0, float x1, float y1,
                   const char* name, ImFont* font, float avg_depth) {
        float name_fs = g_settings.name_font_size;
        ImVec2 ts = font->CalcTextSizeA(name_fs, FLT_MAX, 0,
                                        name, name + strlen(name));

        NamePosition pos = static_cast<NamePosition>(g_settings.name_position);

        // Scale the offset by depth so it's consistent at all distances
        float depth_factor = g_settings.depth_scale / std::max(avg_depth, 1.0f);
        // Clamp so it doesn't go crazy at very close range
        depth_factor = std::clamp(depth_factor, 0.3f, 3.0f);

        float base_gap = 3.0f * depth_factor;
        float offset_x = g_settings.name_offset_x * depth_factor;
        float offset_y = g_settings.name_offset_y * depth_factor;

        float nx = 0, ny = 0;

        switch (pos) {
            case NamePosition::TOP:
                nx = (x0 + x1) * 0.5f - ts.x * 0.5f;
                ny = y0 - ts.y - base_gap;
                break;
            case NamePosition::BOTTOM:
                nx = (x0 + x1) * 0.5f - ts.x * 0.5f;
                ny = y1 + base_gap;
                break;
            case NamePosition::LEFT:
                nx = x0 - ts.x - base_gap * 1.5f;
                ny = (y0 + y1) * 0.5f - ts.y * 0.5f;
                break;
            case NamePosition::RIGHT:
                nx = x1 + base_gap * 1.5f;
                ny = (y0 + y1) * 0.5f - ts.y * 0.5f;
                break;
        }

        nx += offset_x;
        ny += offset_y;

        ImU32 shadow_col = float4_to_col(g_settings.name_shadow_color);
        ImU32 text_col = float4_to_col(g_settings.name_color);

        if (g_settings.name_shadow)
            d->AddText(font, name_fs, {nx + 1, ny + 1}, shadow_col, name);
        d->AddText(font, name_fs, {nx, ny}, text_col, name);
    }
};