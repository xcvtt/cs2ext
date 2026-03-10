#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <string>

struct Vec3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };
struct CBoneData { Vec3 pos; float scale; float quat[4]; };

static constexpr int BONE_HEAD = 6, BONE_NECK = 5, BONE_SPINE1 = 4,
    BONE_SPINE2 = 2, BONE_PELVIS = 0,
    BONE_LSHOULDER = 8, BONE_LELBOW = 9, BONE_LHAND = 10,
    BONE_RSHOULDER = 13, BONE_RELBOW = 14, BONE_RHAND = 15,
    BONE_LHIP = 22, BONE_LKNEE = 23, BONE_LFOOT = 24,
    BONE_RHIP = 25, BONE_RKNEE = 26, BONE_RFOOT = 27;
static constexpr int MAX_BONE = 28;

struct BoneConn { int from, to; };
static constexpr BoneConn CONNECTIONS[] = {
    {BONE_HEAD, BONE_NECK}, {BONE_NECK, BONE_SPINE1},
    {BONE_SPINE1, BONE_SPINE2}, {BONE_SPINE2, BONE_PELVIS},
    {BONE_NECK, BONE_LSHOULDER}, {BONE_LSHOULDER, BONE_LELBOW}, {BONE_LELBOW, BONE_LHAND},
    {BONE_NECK, BONE_RSHOULDER}, {BONE_RSHOULDER, BONE_RELBOW}, {BONE_RELBOW, BONE_RHAND},
    {BONE_PELVIS, BONE_LHIP}, {BONE_LHIP, BONE_LKNEE}, {BONE_LKNEE, BONE_LFOOT},
    {BONE_PELVIS, BONE_RHIP}, {BONE_RHIP, BONE_RKNEE}, {BONE_RKNEE, BONE_RFOOT},
};

static bool w2s(const Vec3& w, const Matrix4x4& vm, int sw, int sh, ImVec2& s) {
    float ww = vm.m[3][0]*w.x + vm.m[3][1]*w.y + vm.m[3][2]*w.z + vm.m[3][3];
    if (ww < 0.001f) return false;
    float inv = 1.0f/ww;
    float x = vm.m[0][0]*w.x + vm.m[0][1]*w.y + vm.m[0][2]*w.z + vm.m[0][3];
    float y = vm.m[1][0]*w.x + vm.m[1][1]*w.y + vm.m[1][2]*w.z + vm.m[1][3];
    s.x = sw*0.5f + x*inv*sw*0.5f;
    s.y = sh*0.5f - y*inv*sh*0.5f;
    return true;
}

static bool w2s_depth(const Vec3& w, const Matrix4x4& vm, int sw, int sh, ImVec2& s, float& depth) {
    float ww = vm.m[3][0]*w.x + vm.m[3][1]*w.y + vm.m[3][2]*w.z + vm.m[3][3];
    if (ww < 0.001f) return false;
    depth = ww;
    float inv = 1.0f/ww;
    float x = vm.m[0][0]*w.x + vm.m[0][1]*w.y + vm.m[0][2]*w.z + vm.m[0][3];
    float y = vm.m[1][0]*w.x + vm.m[1][1]*w.y + vm.m[1][2]*w.z + vm.m[1][3];
    s.x = sw*0.5f + x*inv*sw*0.5f;
    s.y = sh*0.5f - y*inv*sh*0.5f;
    return true;
}

struct PlayerVisuals {
    ImVec2 screens[MAX_BONE];
    float depths[MAX_BONE]{};
    bool visible[MAX_BONE]{};
    int team = 0;
    int health = 0;
    bool valid = false;
    char name[128]{};
    Vec3 origin{};
};

enum class ChamsStyle { FILLED, WIREFRAME, GLOW, FLAT_SHADED };

struct SmoothedBox {
    float min_x=0, min_y=0, max_x=0, max_y=0;
    bool initialized = false;
};

struct MenuSettings;
extern MenuSettings g_settings;
ImU32 float4_to_col(const float c[4]);

// Forward declare for font access
class Menu;
extern Menu g_menu;
class Overlay;
extern Overlay g_overlay;

class VisibleESP {
public:
    enum class BoxStyle { CORNERS, FULL, DASHED };
    SmoothedBox smoothed_boxes[64];

    struct ColorSet {
        ImU32 fill, outline, glow, wire, head_fill;
    };

    ColorSet get_enemy_colors() {
        return {
            float4_to_col(g_settings.enemy_fill),
            float4_to_col(g_settings.enemy_outline),
            float4_to_col(g_settings.enemy_glow),
            float4_to_col(g_settings.enemy_outline),
            (float4_to_col(g_settings.enemy_fill) & 0x00FFFFFF) | 0x60000000,
        };
    }
    ColorSet get_team_colors() {
        return {
            float4_to_col(g_settings.team_fill),
            float4_to_col(g_settings.team_outline),
            float4_to_col(g_settings.team_glow),
            float4_to_col(g_settings.team_outline),
            (float4_to_col(g_settings.team_fill) & 0x00FFFFFF) | 0x60000000,
        };
    }

    void draw_player(ImDrawList* draw, const PlayerVisuals& p, int local_team,
                     int sw, int sh, int idx, bool is_scoped) {
        if (!p.valid || !g_settings.esp_enabled || !g_settings.master_switch) return;
        bool enemy = (p.team != local_team);
        if (!enemy && !g_settings.draw_teammates) return;

        ColorSet c = enemy ? get_enemy_colors() : get_team_colors();
        ChamsStyle style = static_cast<ChamsStyle>(g_settings.chams_style);

        float depth_scale_use = g_settings.depth_scale;
        if (is_scoped) depth_scale_use *= 2.0f;
        float saved_ds = g_settings.depth_scale;
        g_settings.depth_scale = depth_scale_use;

        switch (style) {
        case ChamsStyle::FILLED:
            draw_body_filled(draw, p, c, false);
            draw_body_outline(draw, p, c);
            draw_head(draw, p, c, false);
            break;
        case ChamsStyle::WIREFRAME:
            draw_body_wireframe(draw, p, c);
            draw_head_wire(draw, p, c);
            break;
        case ChamsStyle::GLOW:
            draw_body_glow(draw, p, c, g_settings.glow_expand_outer);
            draw_body_glow(draw, p, c, g_settings.glow_expand_inner);
            draw_body_filled(draw, p, c, false);
            draw_body_outline(draw, p, c);
            draw_head(draw, p, c, true);
            break;
        case ChamsStyle::FLAT_SHADED:
            draw_body_filled(draw, p, c, true);
            draw_body_outline(draw, p, c);
            draw_head(draw, p, c, false);
            break;
        }

        g_settings.depth_scale = saved_ds;

        if (g_settings.draw_skeleton_wire)
            draw_skeleton(draw, p, c);
        if (g_settings.draw_box || g_settings.draw_healthbar ||
            g_settings.draw_health_text || g_settings.draw_name)
            draw_box_hp_name(draw, p, c, idx, is_scoped);
    }

    void reset_smoothing() {
        for (auto& b : smoothed_boxes) b.initialized = false;
    }

private:
    struct LimbDef { int bone_a, bone_b, width_idx; };
    static constexpr LimbDef BODY_MESH[] = {
        {BONE_NECK,BONE_SPINE1,0},{BONE_SPINE1,BONE_SPINE2,1},
        {BONE_SPINE2,BONE_PELVIS,2},
        {BONE_LSHOULDER,BONE_LELBOW,3},{BONE_LELBOW,BONE_LHAND,4},
        {BONE_RSHOULDER,BONE_RELBOW,5},{BONE_RELBOW,BONE_RHAND,6},
        {BONE_LHIP,BONE_LKNEE,7},{BONE_LKNEE,BONE_LFOOT,8},
        {BONE_RHIP,BONE_RKNEE,9},{BONE_RKNEE,BONE_RFOOT,10},
        {BONE_NECK,BONE_LSHOULDER,11},{BONE_NECK,BONE_RSHOULDER,12},
        {BONE_PELVIS,BONE_LHIP,13},{BONE_PELVIS,BONE_RHIP,14},
    };

    struct LimbQuad { ImVec2 p[4]; bool valid; };

    LimbQuad build_limb_quad(const PlayerVisuals& p, const LimbDef& limb, float extra = 0) {
        LimbQuad q{}; q.valid = false;
        if (!p.visible[limb.bone_a] || !p.visible[limb.bone_b]) return q;
        ImVec2 a = p.screens[limb.bone_a], b = p.screens[limb.bone_b];
        float da = p.depths[limb.bone_a], db = p.depths[limb.bone_b];
        float sc = g_settings.body_width_scale, ds = g_settings.depth_scale;
        float wa = (g_settings.limb_width_a[limb.width_idx]*sc+extra)*ds/da;
        float wb = (g_settings.limb_width_b[limb.width_idx]*sc+extra)*ds/db;
        float dx = b.x-a.x, dy = b.y-a.y;
        float len = sqrtf(dx*dx+dy*dy);
        if (len < 0.5f) return q;
        float nx = -dy/len, ny = dx/len;
        q.p[0]={a.x+nx*wa,a.y+ny*wa}; q.p[1]={a.x-nx*wa,a.y-ny*wa};
        q.p[2]={b.x-nx*wb,b.y-ny*wb}; q.p[3]={b.x+nx*wb,b.y+ny*wb};
        q.valid = true; return q;
    }

    void draw_body_filled(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c, bool shade) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l); if (!q.valid) continue;
            if (shade) {
                d->AddTriangleFilled(q.p[0],q.p[1],q.p[2],c.fill);
                d->AddTriangleFilled(q.p[0],q.p[2],q.p[3],darken(c.fill,0.5f));
            } else d->AddQuadFilled(q.p[0],q.p[1],q.p[2],q.p[3],c.fill);
        }
    }
    void draw_body_outline(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l); if (!q.valid) continue;
            d->AddLine(q.p[0],q.p[3],c.outline,1.2f);
            d->AddLine(q.p[1],q.p[2],c.outline,1.2f);
        }
    }
    void draw_body_wireframe(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l); if (!q.valid) continue;
            d->AddLine(q.p[0],q.p[3],c.wire,1.5f);
            d->AddLine(q.p[1],q.p[2],c.wire,1.5f);
            d->AddLine(q.p[0],q.p[1],c.wire,1.0f);
            d->AddLine(q.p[2],q.p[3],c.wire,1.0f);
            d->AddLine(q.p[0],q.p[2],c.wire,0.7f);
            d->AddQuadFilled(q.p[0],q.p[1],q.p[2],q.p[3],(c.fill&0x00FFFFFF)|0x15000000);
        }
    }
    void draw_body_glow(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c, float exp) {
        for (const auto& l : BODY_MESH) {
            auto q = build_limb_quad(p, l, exp); if (!q.valid) continue;
            d->AddQuadFilled(q.p[0],q.p[1],q.p[2],q.p[3],c.glow);
        }
    }
    void draw_head(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c, bool glow) {
        if (!p.visible[BONE_HEAD]||!p.visible[BONE_NECK]) return;
        float r = g_settings.head_radius*g_settings.depth_scale/p.depths[BONE_HEAD];
        r = std::clamp(r,2.0f,80.0f);
        ImVec2 ctr = p.screens[BONE_HEAD];
        if (glow) {
            d->AddCircleFilled(ctr,r+g_settings.glow_expand_outer,c.glow,20);
            d->AddCircleFilled(ctr,r+g_settings.glow_expand_inner,c.glow,20);
        }
        d->AddCircleFilled(ctr,r,c.head_fill,20);
        d->AddCircle(ctr,r,c.outline,20,1.2f);
    }
    void draw_head_wire(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        if (!p.visible[BONE_HEAD]||!p.visible[BONE_NECK]) return;
        float r = g_settings.head_radius*g_settings.depth_scale/p.depths[BONE_HEAD];
        r = std::clamp(r,2.0f,80.0f);
        ImVec2 ctr = p.screens[BONE_HEAD];
        d->AddCircle(ctr,r,c.wire,16,1.5f);
        d->AddLine({ctr.x-r,ctr.y},{ctr.x+r,ctr.y},c.wire,0.7f);
        d->AddLine({ctr.x,ctr.y-r},{ctr.x,ctr.y+r},c.wire,0.7f);
    }
    void draw_skeleton(ImDrawList* d, const PlayerVisuals& p, const ColorSet& c) {
        ImU32 w = (c.wire&0x00FFFFFF)|0x60000000;
        for (const auto& [f,t] : CONNECTIONS)
            if (p.visible[f]&&p.visible[t]) d->AddLine(p.screens[f],p.screens[t],w,1.0f);
    }

void draw_box_hp_name(ImDrawList* d, const PlayerVisuals& p,
                      const ColorSet& c, int idx, bool is_scoped) {
    static constexpr int STABLE[] = {
        BONE_HEAD, BONE_NECK, BONE_SPINE1, BONE_SPINE2, BONE_PELVIS,
        BONE_LSHOULDER, BONE_RSHOULDER, BONE_LHIP, BONE_RHIP,
        BONE_LKNEE, BONE_RKNEE,
    };
    float rmin_x=1e9f, rmin_y=1e9f, rmax_x=-1e9f, rmax_y=-1e9f;
    float avg_depth=0; int cnt=0;
    for (int b : STABLE) {
        if (!p.visible[b]) continue;
        rmin_x = std::min(rmin_x, p.screens[b].x);
        rmin_y = std::min(rmin_y, p.screens[b].y);
        rmax_x = std::max(rmax_x, p.screens[b].x);
        rmax_y = std::max(rmax_y, p.screens[b].y);
        avg_depth += p.depths[b]; cnt++;
    }
    if (cnt < 3) return;
    avg_depth /= cnt;

    float ds = g_settings.depth_scale;
    if (is_scoped) ds *= 2.0f;
    float pad_x = g_settings.box_padding_x * ds / avg_depth;
    float pad_y = g_settings.box_padding_y * ds / avg_depth;
    rmin_x -= pad_x; rmax_x += pad_x;
    rmin_y -= pad_y * 0.6f; rmax_y += pad_y * 0.4f;

    auto& sb = smoothed_boxes[idx];
    float s = g_settings.box_smoothing;
    if (!sb.initialized) {
        sb = {rmin_x, rmin_y, rmax_x, rmax_y, true};
    } else {
        sb.min_x = sb.min_x*s + rmin_x*(1-s);
        sb.min_y = sb.min_y*s + rmin_y*(1-s);
        sb.max_x = sb.max_x*s + rmax_x*(1-s);
        sb.max_y = sb.max_y*s + rmax_y*(1-s);
        sb.min_x = std::min(sb.min_x, rmin_x);
        sb.min_y = std::min(sb.min_y, rmin_y);
        sb.max_x = std::max(sb.max_x, rmax_x);
        sb.max_y = std::max(sb.max_y, rmax_y);
    }

    float x0=sb.min_x, y0=sb.min_y, x1=sb.max_x, y1=sb.max_y;
    float box_thick = g_settings.box_thickness;

    ImFont* font = g_overlay.esp_font;
    float font_size = g_settings.esp_font_size;

    if (g_settings.draw_box) {
        BoxStyle style = static_cast<BoxStyle>(g_settings.box_style);
        ImU32 bg = IM_COL32(0, 0, 0, 80);

        switch (style) {
        case BoxStyle::CORNERS: {
            float w = x1-x0, h = y1-y0;
            float corner = std::min(w, h) * g_settings.box_corner_pct;
            auto corners = [&](ImU32 col, float t) {
                d->AddLine({x0,y0},{x0+corner,y0},col,t);
                d->AddLine({x0,y0},{x0,y0+corner},col,t);
                d->AddLine({x1,y0},{x1-corner,y0},col,t);
                d->AddLine({x1,y0},{x1,y0+corner},col,t);
                d->AddLine({x0,y1},{x0+corner,y1},col,t);
                d->AddLine({x0,y1},{x0,y1-corner},col,t);
                d->AddLine({x1,y1},{x1-corner,y1},col,t);
                d->AddLine({x1,y1},{x1,y1-corner},col,t);
            };
            corners(bg, box_thick + 2);
            corners(c.outline, box_thick);
            break;
        }
        case BoxStyle::FULL: {
            // Outer shadow
            d->AddRect({x0-1,y0-1},{x1+1,y1+1}, bg, 0, 0, box_thick + 2);
            // Main box
            d->AddRect({x0,y0},{x1,y1}, c.outline, 0, 0, box_thick);
            break;
        }
        case BoxStyle::DASHED: {
            float w = x1-x0, h = y1-y0;
            float dash = 8.0f;
            float gap = 5.0f;

            auto dashed_line = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
                float dx = b.x-a.x, dy = b.y-a.y;
                float len = sqrtf(dx*dx + dy*dy);
                if (len < 1) return;
                float nx = dx/len, ny = dy/len;
                float pos = 0;
                while (pos < len) {
                    float end = std::min(pos + dash, len);
                    d->AddLine(
                        {a.x + nx*pos, a.y + ny*pos},
                        {a.x + nx*end, a.y + ny*end},
                        col, thick);
                    pos = end + gap;
                }
            };

            // Shadow
            dashed_line({x0,y0},{x1,y0}, bg, box_thick+2);
            dashed_line({x1,y0},{x1,y1}, bg, box_thick+2);
            dashed_line({x1,y1},{x0,y1}, bg, box_thick+2);
            dashed_line({x0,y1},{x0,y0}, bg, box_thick+2);
            // Main
            dashed_line({x0,y0},{x1,y0}, c.outline, box_thick);
            dashed_line({x1,y0},{x1,y1}, c.outline, box_thick);
            dashed_line({x1,y1},{x0,y1}, c.outline, box_thick);
            dashed_line({x0,y1},{x0,y0}, c.outline, box_thick);
            break;
        }
        }
    }

    if (g_settings.draw_healthbar) {
        float bw=3, bx=x0-bw-4, bh=y1-y0;
        float hp=std::clamp(p.health/100.0f,0.0f,1.0f);
        float filled=bh*hp;
        d->AddRectFilled({bx-1,y0-1},{bx+bw+1,y1+1},IM_COL32(0,0,0,140));
        d->AddRectFilled({bx,y0},{bx+bw,y1},IM_COL32(30,30,30,180));
        uint8_t r=(uint8_t)(255*(1-hp)), g=(uint8_t)(255*hp);
        d->AddRectFilled({bx,y0+(bh-filled)},{bx+bw,y1},IM_COL32(r,g,0,230));

        if (g_settings.draw_health_text && p.health < 100 && font) {
            char txt[8]; snprintf(txt,8,"%d",p.health);
            ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0, txt);
            float tx = bx - ts.x - 2;
            float ty = y0 + (bh-filled) - ts.y * 0.5f;
            d->AddText(font, font_size, {tx+1,ty+1}, IM_COL32(0,0,0,180), txt);
            d->AddText(font, font_size, {tx,ty}, IM_COL32(255,255,255,220), txt);
        }
    }

    if (g_settings.draw_name && p.name[0] && font) {
        ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0,
            p.name, p.name + strlen(p.name));
        float nx = (x0+x1)*0.5f - ts.x*0.5f;
        float ny = y0 - ts.y - 3;
        d->AddText(font, font_size, {nx+1,ny+1}, IM_COL32(0,0,0,200), p.name);
        d->AddText(font, font_size, {nx,ny}, IM_COL32(255,255,255,240), p.name);
    }
}

    static ImU32 darken(ImU32 col, float f) {
        return IM_COL32(
            (int)(((col>>0)&0xFF)*f),(int)(((col>>8)&0xFF)*f),
            (int)(((col>>16)&0xFF)*f),(col>>24)&0xFF);
    }
};

inline VisibleESP g_esp;