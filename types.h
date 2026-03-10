#pragma once
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <cstdint>

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
static constexpr BoneConn SKELETON_CONNECTIONS[] = {
    {BONE_HEAD, BONE_NECK}, {BONE_NECK, BONE_SPINE1},
    {BONE_SPINE1, BONE_SPINE2}, {BONE_SPINE2, BONE_PELVIS},
    {BONE_NECK, BONE_LSHOULDER}, {BONE_LSHOULDER, BONE_LELBOW}, {BONE_LELBOW, BONE_LHAND},
    {BONE_NECK, BONE_RSHOULDER}, {BONE_RSHOULDER, BONE_RELBOW}, {BONE_RELBOW, BONE_RHAND},
    {BONE_PELVIS, BONE_LHIP}, {BONE_LHIP, BONE_LKNEE}, {BONE_LKNEE, BONE_LFOOT},
    {BONE_PELVIS, BONE_RHIP}, {BONE_RHIP, BONE_RKNEE}, {BONE_RKNEE, BONE_RFOOT},
};

struct LimbDef { int bone_a, bone_b, width_idx; };
static constexpr LimbDef BODY_MESH[] = {
    {BONE_NECK, BONE_SPINE1, 0}, {BONE_SPINE1, BONE_SPINE2, 1},
    {BONE_SPINE2, BONE_PELVIS, 2},
    {BONE_LSHOULDER, BONE_LELBOW, 3}, {BONE_LELBOW, BONE_LHAND, 4},
    {BONE_RSHOULDER, BONE_RELBOW, 5}, {BONE_RELBOW, BONE_RHAND, 6},
    {BONE_LHIP, BONE_LKNEE, 7}, {BONE_LKNEE, BONE_LFOOT, 8},
    {BONE_RHIP, BONE_RKNEE, 9}, {BONE_RKNEE, BONE_RFOOT, 10},
    {BONE_NECK, BONE_LSHOULDER, 11}, {BONE_NECK, BONE_RSHOULDER, 12},
    {BONE_PELVIS, BONE_LHIP, 13}, {BONE_PELVIS, BONE_RHIP, 14},
};

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