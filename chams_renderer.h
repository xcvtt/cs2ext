#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "types.h"
#include "settings.h"

enum class ChamsStyle { FILLED, WIREFRAME, GLOW, FLAT_SHADED };

struct ColorSet {
    ImU32 fill, outline, glow, wire, head_fill;
};

inline ColorSet get_enemy_colors() {
    return {
        float4_to_col(g_settings.enemy_fill),
        float4_to_col(g_settings.enemy_outline),
        float4_to_col(g_settings.enemy_glow),
        float4_to_col(g_settings.enemy_outline),
        (float4_to_col(g_settings.enemy_fill) & 0x00FFFFFF) | 0x60000000,
    };
}

inline ColorSet get_team_colors() {
    return {
        float4_to_col(g_settings.team_fill),
        float4_to_col(g_settings.team_outline),
        float4_to_col(g_settings.team_glow),
        float4_to_col(g_settings.team_outline),
        (float4_to_col(g_settings.team_fill) & 0x00FFFFFF) | 0x60000000,
    };
}

// A tapered quad representing a limb segment
struct LimbQuad {
    ImVec2 p[4]; // p[0],p[1] = bone_a end; p[2],p[3] = bone_b end
    bool valid;
};

// An ellipse projected on screen
struct ScreenEllipse {
    ImVec2 center;
    float rx, ry; // screen-space radii
    float angle;  // rotation in radians
    bool valid;
};

class ChamsRenderer {
public:
    // Build a tapered quad between two bones with perspective-correct widths
    static LimbQuad build_limb_quad(const PlayerVisuals& p, const LimbDef& limb,
                                    float depth_scale, float extra = 0) {
        LimbQuad q{};
        q.valid = false;
        if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) return q;

        ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
        float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];

        float sc = g_settings.body_width_scale;
        float wa = (limb.width_a * sc + extra) * depth_scale / da;
        float wb = (limb.width_b * sc + extra) * depth_scale / db;

        float dx = b.x - a.x, dy = b.y - a.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.5f) return q;

        float nx = -dy / len, ny = dx / len;
        q.p[0] = {a.x + nx * wa, a.y + ny * wa};
        q.p[1] = {a.x - nx * wa, a.y - ny * wa};
        q.p[2] = {b.x - nx * wb, b.y - ny * wb};
        q.p[3] = {b.x + nx * wb, b.y + ny * wb};
        q.valid = true;
        return q;
    }

    // Draw an ellipse at a bone position
    static void draw_ellipse_filled(ImDrawList* d, ImVec2 center,
                                     float rx, float ry, float angle,
                                     ImU32 col, int segments = 20) {
        if (rx < 0.5f || ry < 0.5f) return;
        float cos_a = cosf(angle), sin_a = sinf(angle);
        ImVec2 prev;
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments * 6.28318530f;
            float ex = rx * cosf(t);
            float ey = ry * sinf(t);
            ImVec2 pt = {
                center.x + ex * cos_a - ey * sin_a,
                center.y + ex * sin_a + ey * cos_a
            };
            if (i > 0) {
                d->AddTriangleFilled(center, prev, pt, col);
            }
            prev = pt;
        }
    }

    static void draw_ellipse_stroke(ImDrawList* d, ImVec2 center,
                                     float rx, float ry, float angle,
                                     ImU32 col, float thickness = 1.0f,
                                     int segments = 20) {
        if (rx < 0.5f || ry < 0.5f) return;
        float cos_a = cosf(angle), sin_a = sinf(angle);
        ImVec2 first, prev;
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments * 6.28318530f;
            float ex = rx * cosf(t);
            float ey = ry * sinf(t);
            ImVec2 pt = {
                center.x + ex * cos_a - ey * sin_a,
                center.y + ex * sin_a + ey * cos_a
            };
            if (i == 0) first = pt;
            else d->AddLine(prev, pt, col, thickness);
            prev = pt;
        }
    }

    // Draw a rounded/elliptical limb segment between two bones
    // Uses multiple ellipses along the limb to create a smooth tube effect
    static void draw_limb_tube(ImDrawList* d, const PlayerVisuals& p,
                                const LimbDef& limb, float depth_scale,
                                ImU32 fill_col, int slices = 6) {
        if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) return;

        ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
        float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
        float sc = g_settings.body_width_scale;

        float dx = b.x - a.x, dy = b.y - a.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) return;

        float angle = atan2f(dy, dx);

        // Draw filled ellipses along the limb
        for (int i = 0; i <= slices; i++) {
            float t = (float)i / slices;
            ImVec2 pos = {a.x + dx * t, a.y + dy * t};
            float depth = da + (db - da) * t;
            float w = (limb.width_a + (limb.width_b - limb.width_a) * t) * sc;

            float rx = w * depth_scale / depth;
            float ry = rx * 0.55f; // slightly flattened for natural look

            draw_ellipse_filled(d, pos, rx, ry, angle, fill_col, 16);
        }
    }

    // ===== BODY RENDERING STYLES =====

    static void draw_body_filled(ImDrawList* d, const PlayerVisuals& p,
                                  const ColorSet& c, bool shade, float depth_scale) {
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            auto q = build_limb_quad(p, BODY_LIMBS[i], depth_scale);
            if (!q.valid) continue;
            if (shade) {
                d->AddTriangleFilled(q.p[0], q.p[1], q.p[2], c.fill);
                d->AddTriangleFilled(q.p[0], q.p[2], q.p[3], darken(c.fill, 0.5f));
            } else {
                d->AddQuadFilled(q.p[0], q.p[1], q.p[2], q.p[3], c.fill);
            }
        }
    }

    static void draw_body_outline(ImDrawList* d, const PlayerVisuals& p,
                                   const ColorSet& c, float depth_scale) {
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            auto q = build_limb_quad(p, BODY_LIMBS[i], depth_scale);
            if (!q.valid) continue;
            d->AddLine(q.p[0], q.p[3], c.outline, 1.2f);
            d->AddLine(q.p[1], q.p[2], c.outline, 1.2f);
        }
    }

    static void draw_body_wireframe(ImDrawList* d, const PlayerVisuals& p,
                                     const ColorSet& c, float depth_scale) {
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            auto q = build_limb_quad(p, BODY_LIMBS[i], depth_scale);
            if (!q.valid) continue;
            // Outer edges
            d->AddLine(q.p[0], q.p[3], c.wire, 1.5f);
            d->AddLine(q.p[1], q.p[2], c.wire, 1.5f);
            // End caps
            d->AddLine(q.p[0], q.p[1], c.wire, 1.0f);
            d->AddLine(q.p[2], q.p[3], c.wire, 1.0f);
            // Cross brace
            d->AddLine(q.p[0], q.p[2], c.wire, 0.7f);
            // Subtle fill
            d->AddQuadFilled(q.p[0], q.p[1], q.p[2], q.p[3],
                             (c.fill & 0x00FFFFFF) | 0x15000000);
        }

        // Add joint circles at key bones for mechanical/wireframe look
        static constexpr int joints[] = {
            BONE_NECK, BONE_SPINE1, BONE_PELVIS,
            BONE_LSHOULDER, BONE_LELBOW, BONE_LHAND,
            BONE_RSHOULDER, BONE_RELBOW, BONE_RHAND,
            BONE_LHIP, BONE_LKNEE, BONE_LFOOT,
            BONE_RHIP, BONE_RKNEE, BONE_RFOOT,
        };
        for (int b : joints) {
            if (!p.visible[b]) continue;
            float r = 2.5f * g_settings.body_width_scale * depth_scale / p.depths[b];
            r = std::clamp(r, 1.5f, 8.0f);
            d->AddCircle(p.screens[b], r, c.wire, 8, 1.0f);
        }
    }

    static void draw_body_glow(ImDrawList* d, const PlayerVisuals& p,
                                const ColorSet& c, float depth_scale, float expand) {
        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            auto q = build_limb_quad(p, BODY_LIMBS[i], depth_scale, expand);
            if (!q.valid) continue;
            d->AddQuadFilled(q.p[0], q.p[1], q.p[2], q.p[3], c.glow);
        }
    }

    // New outline style: smooth elliptical body silhouette
    static void draw_body_outline_smooth(ImDrawList* d, const PlayerVisuals& p,
                                          const ColorSet& c, float depth_scale) {
        float sc = g_settings.body_width_scale;

        for (int i = 0; i < BODY_LIMB_COUNT; i++) {
            const auto& limb = BODY_LIMBS[i];
            if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) continue;

            ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
            float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];

            float dx = b.x - a.x, dy = b.y - a.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.0f) continue;

            // Midpoint ellipse representing the limb
            ImVec2 mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            float mid_depth = (da + db) * 0.5f;
            float avg_w = (limb.width_a + limb.width_b) * 0.5f * sc;

            float rx = avg_w * depth_scale / mid_depth;
            float ry = len * 0.5f;
            float angle = atan2f(dy, dx);

            // Fill
            draw_ellipse_filled(d, mid, rx, ry, angle,
                                (c.fill & 0x00FFFFFF) | 0x30000000, 20);
            // Outline
            draw_ellipse_stroke(d, mid, rx, ry, angle, c.outline, 1.5f, 20);
        }
    }

    // ===== HEAD RENDERING =====

    static void draw_head(ImDrawList* d, const PlayerVisuals& p,
                          const ColorSet& c, bool glow, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;

        float r = g_settings.head_radius * depth_scale / p.depths[BONE_HEAD];
        r = std::clamp(r, 2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];
        ImVec2 neck = p.screens[BONE_NECK];

        // Neck connection: small tapered quad from head to neck
        float neck_w_top = r * 0.5f;
        float neck_w_bot = r * 0.7f;
        float dx = neck.x - head.x, dy = neck.y - head.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 1.0f) {
            float nx = -dy / len, ny = dx / len;
            ImVec2 nt[4] = {
                {head.x + nx * neck_w_top, head.y + ny * neck_w_top},
                {head.x - nx * neck_w_top, head.y - ny * neck_w_top},
                {neck.x - nx * neck_w_bot, neck.y - ny * neck_w_bot},
                {neck.x + nx * neck_w_bot, neck.y + ny * neck_w_bot},
            };
            if (glow) {
                float expand = g_settings.glow_expand_inner;
                ImVec2 ntg[4] = {
                    {head.x + nx * (neck_w_top + expand), head.y + ny * (neck_w_top + expand)},
                    {head.x - nx * (neck_w_top + expand), head.y - ny * (neck_w_top + expand)},
                    {neck.x - nx * (neck_w_bot + expand), neck.y - ny * (neck_w_bot + expand)},
                    {neck.x + nx * (neck_w_bot + expand), neck.y + ny * (neck_w_bot + expand)},
                };
                d->AddQuadFilled(ntg[0], ntg[1], ntg[2], ntg[3], c.glow);
            }
            d->AddQuadFilled(nt[0], nt[1], nt[2], nt[3], c.head_fill);
            d->AddLine(nt[0], nt[3], c.outline, 1.0f);
            d->AddLine(nt[1], nt[2], c.outline, 1.0f);
        }

        // Glow rings
        if (glow) {
            d->AddCircleFilled(head, r + g_settings.glow_expand_outer, c.glow, 24);
            d->AddCircleFilled(head, r + g_settings.glow_expand_inner, c.glow, 24);
        }

        // Head: slightly oval (taller than wide)
        float head_rx = r;
        float head_ry = r * 1.15f;
        draw_ellipse_filled(d, head, head_rx, head_ry, 0, c.head_fill, 24);
        draw_ellipse_stroke(d, head, head_rx, head_ry, 0, c.outline, 1.2f, 24);
    }

    static void draw_head_wire(ImDrawList* d, const PlayerVisuals& p,
                                const ColorSet& c, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;

        float r = g_settings.head_radius * depth_scale / p.depths[BONE_HEAD];
        r = std::clamp(r, 2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];

        float head_rx = r;
        float head_ry = r * 1.15f;
        draw_ellipse_stroke(d, head, head_rx, head_ry, 0, c.wire, 1.5f, 20);

        // Cross-hair inside head
        d->AddLine({head.x - head_rx, head.y}, {head.x + head_rx, head.y}, c.wire, 0.7f);
        d->AddLine({head.x, head.y - head_ry}, {head.x, head.y + head_ry}, c.wire, 0.7f);

        // Neck line
        ImVec2 neck = p.screens[BONE_NECK];
        d->AddLine(head, neck, c.wire, 1.0f);
    }

    static void draw_head_outline(ImDrawList* d, const PlayerVisuals& p,
                                   const ColorSet& c, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;

        float r = g_settings.head_radius * depth_scale / p.depths[BONE_HEAD];
        r = std::clamp(r, 2.0f, 80.0f);
        ImVec2 head = p.screens[BONE_HEAD];

        float head_rx = r;
        float head_ry = r * 1.15f;

        draw_ellipse_filled(d, head, head_rx, head_ry, 0,
                            (c.fill & 0x00FFFFFF) | 0x30000000, 24);
        draw_ellipse_stroke(d, head, head_rx, head_ry, 0, c.outline, 1.5f, 24);
    }

    // ===== SKELETON =====

    static void draw_skeleton(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        ImU32 w = (c.wire & 0x00FFFFFF) | 0x60000000;
        for (const auto& [f, t] : SKELETON_CONNECTIONS)
            if (p.visible[f] && p.visible[t])
                d->AddLine(p.screens[f], p.screens[t], w, 1.0f);
    }

    // ===== MAIN DISPATCH =====

    static void draw_chams(ImDrawList* d, const PlayerVisuals& p,
                           const ColorSet& c, ChamsStyle style, float depth_scale) {
        switch (style) {
        case ChamsStyle::FILLED:
            draw_body_filled(d, p, c, false, depth_scale);
            draw_body_outline(d, p, c, depth_scale);
            draw_head(d, p, c, false, depth_scale);
            break;
        case ChamsStyle::WIREFRAME:
            draw_body_wireframe(d, p, c, depth_scale);
            draw_head_wire(d, p, c, depth_scale);
            break;
        case ChamsStyle::GLOW:
            draw_body_glow(d, p, c, depth_scale, g_settings.glow_expand_outer);
            draw_body_glow(d, p, c, depth_scale, g_settings.glow_expand_inner);
            draw_body_filled(d, p, c, false, depth_scale);
            draw_body_outline(d, p, c, depth_scale);
            draw_head(d, p, c, true, depth_scale);
            break;
        case ChamsStyle::FLAT_SHADED:
            draw_body_filled(d, p, c, true, depth_scale);
            draw_body_outline(d, p, c, depth_scale);
            draw_head(d, p, c, false, depth_scale);
            break;
        }
    }
};