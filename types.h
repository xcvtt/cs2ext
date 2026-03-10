#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <cstdint>

struct Vec3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };
struct CBoneData { Vec3 pos; float scale; float quat[4]; };

// Bone indices
static constexpr int BONE_HEAD = 6, BONE_NECK = 5, BONE_SPINE1 = 4,
    BONE_SPINE2 = 2, BONE_PELVIS = 0,
    BONE_LSHOULDER = 8, BONE_LELBOW = 9, BONE_LHAND = 10,
    BONE_RSHOULDER = 13, BONE_RELBOW = 14, BONE_RHAND = 15,
    BONE_LHIP = 22, BONE_LKNEE = 23, BONE_LFOOT = 24,
    BONE_RHIP = 25, BONE_RKNEE = 26, BONE_RFOOT = 27;
static constexpr int MAX_BONE = 28;

// Simple skeleton connections for wire mode
struct BoneConn { int from, to; };
static constexpr BoneConn SKELETON_CONNECTIONS[] = {
    {BONE_HEAD, BONE_NECK}, {BONE_NECK, BONE_SPINE1},
    {BONE_SPINE1, BONE_SPINE2}, {BONE_SPINE2, BONE_PELVIS},
    {BONE_NECK, BONE_LSHOULDER}, {BONE_LSHOULDER, BONE_LELBOW}, {BONE_LELBOW, BONE_LHAND},
    {BONE_NECK, BONE_RSHOULDER}, {BONE_RSHOULDER, BONE_RELBOW}, {BONE_RELBOW, BONE_RHAND},
    {BONE_PELVIS, BONE_LHIP}, {BONE_LHIP, BONE_LKNEE}, {BONE_LKNEE, BONE_LFOOT},
    {BONE_PELVIS, BONE_RHIP}, {BONE_RHIP, BONE_RKNEE}, {BONE_RKNEE, BONE_RFOOT},
};

// Anatomical limb definitions with natural width proportions
// width_a = width at bone_a end, width_b = width at bone_b end (in world units)
struct LimbDef {
    int bone_a, bone_b;
    float width_a, width_b;  // base widths before scaling
};

// Proportions based on CS2 character model anatomy
// Torso is wider, limbs taper toward extremities
static constexpr LimbDef BODY_LIMBS[] = {
    // Torso (wider, barrel-shaped)
    {BONE_NECK,    BONE_SPINE1,  7.0f, 8.0f},   // upper chest
    {BONE_SPINE1,  BONE_SPINE2,  8.0f, 7.5f},    // lower chest
    {BONE_SPINE2,  BONE_PELVIS,  7.5f, 7.0f},    // abdomen

    // Arms (taper from shoulder to hand)
    {BONE_LSHOULDER, BONE_LELBOW, 3.8f, 3.2f},   // left upper arm
    {BONE_LELBOW,    BONE_LHAND,  3.2f, 2.2f},    // left forearm
    {BONE_RSHOULDER, BONE_RELBOW, 3.8f, 3.2f},    // right upper arm
    {BONE_RELBOW,    BONE_RHAND,  3.2f, 2.2f},    // right forearm

    // Legs (thicker at thigh, taper to ankle)
    {BONE_LHIP,  BONE_LKNEE,  5.0f, 3.8f},       // left thigh
    {BONE_LKNEE, BONE_LFOOT,  3.8f, 2.8f},        // left shin
    {BONE_RHIP,  BONE_RKNEE,  5.0f, 3.8f},        // right thigh
    {BONE_RKNEE, BONE_RFOOT,  3.8f, 2.8f},        // right shin

    // Shoulder bridges (connect neck to shoulders)
    {BONE_NECK, BONE_LSHOULDER, 4.0f, 3.8f},
    {BONE_NECK, BONE_RSHOULDER, 4.0f, 3.8f},

    // Hip bridges (connect pelvis to hips)
    {BONE_PELVIS, BONE_LHIP, 5.5f, 5.0f},
    {BONE_PELVIS, BONE_RHIP, 5.5f, 5.0f},
};
static constexpr int BODY_LIMB_COUNT = sizeof(BODY_LIMBS) / sizeof(BODY_LIMBS[0]);

// Bones used for bounding box calculation
static constexpr int BOX_STABLE_BONES[] = {
    BONE_HEAD, BONE_NECK, BONE_SPINE1, BONE_SPINE2, BONE_PELVIS,
    BONE_LSHOULDER, BONE_RSHOULDER, BONE_LHIP, BONE_RHIP,
    BONE_LKNEE, BONE_RKNEE,
};

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

struct RadarPlayer {
    float x, y, z;
    int team;
    int health;
    bool valid;
    char name[128];
};

inline ImU32 float4_to_col(const float c[4]) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255),
                    (int)(c[2] * 255), (int)(c[3] * 255));
}

inline ImU32 darken(ImU32 col, float f) {
    return IM_COL32(
        (int)(((col >> 0) & 0xFF) * f), (int)(((col >> 8) & 0xFF) * f),
        (int)(((col >> 16) & 0xFF) * f), (col >> 24) & 0xFF);
}

inline ImU32 alpha_mod(ImU32 col, float a) {
    int alpha = (int)(((col >> 24) & 0xFF) * a);
    return (col & 0x00FFFFFF) | ((alpha & 0xFF) << 24);
}

inline bool w2s(const Vec3& w, const Matrix4x4& vm, int sw, int sh, ImVec2& s) {
    float ww = vm.m[3][0] * w.x + vm.m[3][1] * w.y + vm.m[3][2] * w.z + vm.m[3][3];
    if (ww < 0.001f) return false;
    float inv = 1.0f / ww;
    float x = vm.m[0][0] * w.x + vm.m[0][1] * w.y + vm.m[0][2] * w.z + vm.m[0][3];
    float y = vm.m[1][0] * w.x + vm.m[1][1] * w.y + vm.m[1][2] * w.z + vm.m[1][3];
    s.x = sw * 0.5f + x * inv * sw * 0.5f;
    s.y = sh * 0.5f - y * inv * sh * 0.5f;
    return true;
}

inline bool w2s_depth(const Vec3& w, const Matrix4x4& vm, int sw, int sh,
                      ImVec2& s, float& depth) {
    float ww = vm.m[3][0] * w.x + vm.m[3][1] * w.y + vm.m[3][2] * w.z + vm.m[3][3];
    if (ww < 0.001f) return false;
    depth = ww;
    float inv = 1.0f / ww;
    float x = vm.m[0][0] * w.x + vm.m[0][1] * w.y + vm.m[0][2] * w.z + vm.m[0][3];
    float y = vm.m[1][0] * w.x + vm.m[1][1] * w.y + vm.m[1][2] * w.z + vm.m[1][3];
    s.x = sw * 0.5f + x * inv * sw * 0.5f;
    s.y = sh * 0.5f - y * inv * sh * 0.5f;
    return true;
}