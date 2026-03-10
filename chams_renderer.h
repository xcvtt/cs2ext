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

struct LimbQuad {
    ImVec2 p[4];
    bool valid;
};

class ChamsRenderer {
public:
    static LimbQuad build_limb_quad(const PlayerVisuals& p, const LimbDef& limb,
                                    float depth_scale, float extra = 0) {
        LimbQuad q{};
        q.valid = false;
        if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) return q;
        ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
        float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
        float sc = g_settings.body_width_scale;
        float wa = (g_settings.limb_width_a[limb.width_idx] * sc + extra) * depth_scale / da;
        float wb = (g_settings.limb_width_b[limb.width_idx] * sc + extra) * depth_scale / db;
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

    static void draw_body_filled(ImDrawList* d, const PlayerVisuals& p,
                                 const ColorSet& c, bool shade, float depth_scale) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l, depth_scale);
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
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l, depth_scale);
            if (!q.valid) continue;
            d->AddLine(q.p[0], q.p[3], c.outline, 1.2f);
            d->AddLine(q.p[1], q.p[2], c.outline, 1.2f);
        }
    }

    static void draw_body_wireframe(ImDrawList* d, const PlayerVisuals& p,
                                    const ColorSet& c, float depth_scale) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l, depth_scale);
            if (!q.valid) continue;
            d->AddLine(q.p[0], q.p[3], c.wire, 1.5f);
            d->AddLine(q.p[1], q.p[2], c.wire, 1.5f);
            d->AddLine(q.p[0], q.p[1], c.wire, 1.0f);
            d->AddLine(q.p[2], q.p[3], c.wire, 1.0f);
            d->AddLine(q.p[0], q.p[2], c.wire, 0.7f);
            d->AddQuadFilled(q.p[0], q.p[1], q.p[2], q.p[3],
                             (c.fill & 0x00FFFFFF) | 0x15000000);
        }
    }

    static void draw_body_glow(ImDrawList* d, const PlayerVisuals& p,
                                const ColorSet& c, float depth_scale, float expand) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l, depth_scale, expand);
            if (!q.valid) continue;
            d->AddQuadFilled(q.p[0], q.p[1], q.p[2], q.p[3], c.glow);
        }
    }

    static void draw_head(ImDrawList* d, const PlayerVisuals& p,
                          const ColorSet& c, bool glow, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;
        float r = g_settings.head_radius * depth_scale / p.depths[BONE_HEAD];
        r = std::clamp(r, 2.0f, 80.0f);
        ImVec2 ctr = p.screens[BONE_HEAD];
        if (glow) {
            d->AddCircleFilled(ctr, r + g_settings.glow_expand_outer, c.glow, 20);
            d->AddCircleFilled(ctr, r + g_settings.glow_expand_inner, c.glow, 20);
        }
        d->AddCircleFilled(ctr, r, c.head_fill, 20);
        d->AddCircle(ctr, r, c.outline, 20, 1.2f);
    }

    static void draw_head_wire(ImDrawList* d, const PlayerVisuals& p,
                               const ColorSet& c, float depth_scale) {
        if (!p.visible[BONE_HEAD] || !p.visible[BONE_NECK]) return;
        float r = g_settings.head_radius * depth_scale / p.depths[BONE_HEAD];
        r = std::clamp(r, 2.0f, 80.0f);
        ImVec2 ctr = p.screens[BONE_HEAD];
        d->AddCircle(ctr, r, c.wire, 16, 1.5f);
        d->AddLine({ctr.x - r, ctr.y}, {ctr.x + r, ctr.y}, c.wire, 0.7f);
        d->AddLine({ctr.x, ctr.y - r}, {ctr.x, ctr.y + r}, c.wire, 0.7f);
    }

    static void draw_skeleton(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        ImU32 w = (c.wire & 0x00FFFFFF) | 0x60000000;
        for (const auto& [f, t] : SKELETON_CONNECTIONS)
            if (p.visible[f] && p.visible[t])
                d->AddLine(p.screens[f], p.screens[t], w, 1.0f);
    }

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