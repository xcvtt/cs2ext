#pragma once
#include <imgui.h>
#include <cmath>
#include "types.h"

class Crosshair {
public:
    struct Config {
        bool enabled;
        int shape;
        float size, gap, thickness;
        ImU32 color;
        bool outline;
        float outline_thickness;
        ImU32 outline_color;
        bool dot;
        float dot_size;
    };

    void draw(ImDrawList* d, int sw, int sh, const Config& c) {
        if (!c.enabled) return;
        float cx = floorf(sw * 0.5f);
        float cy = floorf(sh * 0.5f);
        draw_at(d, cx, cy, c);
    }

    void draw_preview(ImDrawList* d, float cx, float cy, const Config& c) {
        draw_at(d, floorf(cx), floorf(cy), c);
    }

private:
    void draw_at(ImDrawList* d, float cx, float cy, const Config& c) {
        switch (c.shape) {
        case 0: draw_cross(d, cx, cy, c, false); break;
        case 1: draw_cross(d, cx, cy, c, true); break;
        case 2: draw_circle(d, cx, cy, c); break;
        case 3: draw_dot_only(d, cx, cy, c); break;
        case 4:
            draw_cross(d, cx, cy, c, false);
            draw_circle(d, cx, cy, c);
            break;
        case 5: draw_diamond(d, cx, cy, c); break;
        case 6: draw_arrows(d, cx, cy, c); break;
        }

        if (c.dot && c.shape != 3) {
            if (c.outline)
                d->AddCircleFilled({cx, cy}, c.dot_size + c.outline_thickness,
                                   c.outline_color, 20);
            d->AddCircleFilled({cx, cy}, c.dot_size, c.color, 20);
        }
    }

    static void draw_cross(ImDrawList* d, float cx, float cy,
                            const Config& c, bool t_shape) {
        float len = c.size;
        float gap = c.gap;
        float ht = c.thickness * 0.5f;

        if (c.outline) {
            float o = c.outline_thickness;
            ImU32 oc = c.outline_color;
            d->AddRectFilled({cx + gap - o, cy - ht - o},
                             {cx + gap + len + o, cy + ht + o}, oc);
            d->AddRectFilled({cx - gap - len - o, cy - ht - o},
                             {cx - gap + o, cy + ht + o}, oc);
            d->AddRectFilled({cx - ht - o, cy + gap - o},
                             {cx + ht + o, cy + gap + len + o}, oc);
            if (!t_shape)
                d->AddRectFilled({cx - ht - o, cy - gap - len - o},
                                 {cx + ht + o, cy - gap + o}, oc);
        }

        ImU32 col = c.color;
        d->AddRectFilled({cx + gap, cy - ht}, {cx + gap + len, cy + ht}, col);
        d->AddRectFilled({cx - gap - len, cy - ht}, {cx - gap, cy + ht}, col);
        d->AddRectFilled({cx - ht, cy + gap}, {cx + ht, cy + gap + len}, col);
        if (!t_shape)
            d->AddRectFilled({cx - ht, cy - gap - len}, {cx + ht, cy - gap}, col);
    }

    static void draw_circle(ImDrawList* d, float cx, float cy, const Config& c) {
        float radius = c.gap + c.size * 0.5f;
        if (c.outline)
            d->AddCircle({cx, cy}, radius, c.outline_color, 32,
                         c.thickness + c.outline_thickness * 2);
        d->AddCircle({cx, cy}, radius, c.color, 32, c.thickness);
    }

    static void draw_dot_only(ImDrawList* d, float cx, float cy, const Config& c) {
        if (c.outline)
            d->AddCircleFilled({cx, cy}, c.size + c.outline_thickness,
                               c.outline_color, 20);
        d->AddCircleFilled({cx, cy}, c.size, c.color, 20);
    }

    static void draw_diamond(ImDrawList* d, float cx, float cy, const Config& c) {
        float s = c.size;
        ImVec2 p[4] = {{cx, cy - s}, {cx + s, cy}, {cx, cy + s}, {cx - s, cy}};
        if (c.outline)
            for (int i = 0; i < 4; i++)
                d->AddLine(p[i], p[(i + 1) % 4], c.outline_color,
                           c.thickness + c.outline_thickness * 2);
        for (int i = 0; i < 4; i++)
            d->AddLine(p[i], p[(i + 1) % 4], c.color, c.thickness);
    }

    static void draw_arrows(ImDrawList* d, float cx, float cy, const Config& c) {
        float s = c.size;
        float g = c.gap;
        float a = s * 0.5f;

        struct Seg {
            ImVec2 a, b;
        };
        Seg segs[] = {
            {{cx + g + s, cy}, {cx + g + s - a, cy - a}},
            {{cx + g + s, cy}, {cx + g + s - a, cy + a}},
            {{cx - g - s, cy}, {cx - g - s + a, cy - a}},
            {{cx - g - s, cy}, {cx - g - s + a, cy + a}},
            {{cx, cy + g + s}, {cx - a, cy + g + s - a}},
            {{cx, cy + g + s}, {cx + a, cy + g + s - a}},
            {{cx, cy - g - s}, {cx - a, cy - g - s + a}},
            {{cx, cy - g - s}, {cx + a, cy - g - s + a}},
        };
        if (c.outline)
            for (auto& seg : segs)
                d->AddLine(seg.a, seg.b, c.outline_color,
                           c.thickness + c.outline_thickness * 2);
        for (auto& seg : segs)
            d->AddLine(seg.a, seg.b, c.color, c.thickness);
    }
};

inline Crosshair g_crosshair;