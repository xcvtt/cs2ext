#pragma once
#include <cmath>
#include <cstring>
#include "types.h"
#include "memory.h"
#include "offsets.h"
#include "entity_utils.h"

struct WeaponInfo {
    uint16_t def_index;
    const char* name;
};

static constexpr WeaponInfo WEAPON_TABLE[] = {
    {1,   "Desert Eagle"},
    {2,   "Dual Berettas"},
    {3,   "Five-SeveN"},
    {4,   "Glock-18"},
    {7,   "AK-47"},
    {8,   "AUG"},
    {9,   "AWP"},
    {10,  "FAMAS"},
    {11,  "G3SG1"},
    {13,  "Galil AR"},
    {14,  "M249"},
    {16,  "M4A4"},
    {17,  "MAC-10"},
    {19,  "P90"},
    {23,  "MP5-SD"},
    {24,  "UMP-45"},
    {25,  "XM1014"},
    {26,  "PP-Bizon"},
    {27,  "MAG-7"},
    {28,  "Negev"},
    {29,  "Sawed-Off"},
    {30,  "Tec-9"},
    {31,  "Zeus x27"},
    {32,  "P2000"},
    {33,  "MP7"},
    {34,  "MP9"},
    {35,  "Nova"},
    {36,  "P250"},
    {38,  "SCAR-20"},
    {39,  "SG 553"},
    {40,  "SSG 08"},
    {42,  "Knife"},
    {43,  "Flashbang"},
    {44,  "HE Grenade"},
    {45,  "Smoke"},
    {46,  "Molotov"},
    {47,  "Decoy"},
    {48,  "Incendiary"},
    {49,  "C4"},
    {59,  "Knife"},
    {60,  "M4A1-S"},
    {61,  "USP-S"},
    {63,  "CZ75-Auto"},
    {64,  "R8 Revolver"},
    {500, "Bayonet"},
    {503, "Shadow Daggers"},
    {505, "Flip Knife"},
    {506, "Gut Knife"},
    {507, "Karambit"},
    {508, "M9 Bayonet"},
    {509, "Huntsman Knife"},
    {512, "Falchion Knife"},
    {514, "Bowie Knife"},
    {515, "Butterfly Knife"},
    {516, "Ursus Knife"},
    {517, "Navaja Knife"},
    {518, "Stiletto Knife"},
    {519, "Talon Knife"},
    {520, "Skeleton Knife"},
    {521, "Nomad Knife"},
    {522, "Survival Knife"},
    {523, "Paracord Knife"},
    {525, "Classic Knife"},
    {526, "Kukri Knife"},
};
static constexpr int WEAPON_TABLE_SIZE = sizeof(WEAPON_TABLE) / sizeof(WEAPON_TABLE[0]);

inline const WeaponInfo* lookup_weapon(uint16_t def_index) {
    for (int i = 0; i < WEAPON_TABLE_SIZE; i++)
        if (WEAPON_TABLE[i].def_index == def_index)
            return &WEAPON_TABLE[i];
    return nullptr;
}

struct LocalPlayerState {
    uintptr_t pawn = 0;
    uintptr_t controller = 0;
    int team = 0;
    float x = 0, y = 0, yaw = 0;
    bool is_scoped = false;
};

struct FrameState {
    Matrix4x4 view_matrix{};
    LocalPlayerState local;
    PlayerVisuals players[64];
    RadarPlayer radar_players[64];
    uintptr_t entity_list = 0;
};

// -----------------------------------------------------------------------
// Compact pawn data we can read in a single bulk RPM call.
// Covers all offsets we need from a pawn pointer.
// We read a window of bytes and index into it locally — zero extra RPM calls
// once we have this buffer.
// -----------------------------------------------------------------------
struct PawnSnapshot {
    // Raw byte buffer covering offset 0 up to the highest offset we need.
    // Highest needed: max of (m_iHealth, m_iTeamNum, m_pGameSceneNode,
    //                         m_bIsScoped, m_pObserverServices, m_pClippingWeapon)
    // These are typically all within the first 0x800 bytes of the pawn.
    static constexpr size_t SIZE = 0x800;
    uint8_t buf[SIZE];

    template<typename T>
    T get(uint32_t offset) const {
        if (offset + sizeof(T) > SIZE) return T{};
        T val;
        memcpy(&val, buf + offset, sizeof(T));
        return val;
    }
};

class EntityReader {
public:
    FrameState read_frame(int screen_w, int screen_h) {
        FrameState state{};

        // --- 1 RPM: view matrix ---
        g_memory.read_raw(g_memory.get_client_base() + g_offsets.client.dwViewMatrix,
                          &state.view_matrix, 64);

        // --- 1 RPM: local pawn ptr ---
        state.local.pawn = g_memory.read<uintptr_t>(
            g_memory.get_client_base() + g_offsets.client.dwLocalPlayerPawn);
        // --- 1 RPM: local controller ptr ---
        state.local.controller = g_memory.read<uintptr_t>(
            g_memory.get_client_base() + g_offsets.client.dwLocalPlayerController);

        if (state.local.pawn) {
            // --- 1 RPM: bulk-read local pawn snapshot ---
            PawnSnapshot local_snap{};
            g_memory.read_raw(state.local.pawn, local_snap.buf, PawnSnapshot::SIZE);

            state.local.team = local_snap.get<int>(g_offsets.C_BaseEntity.m_iTeamNum);
            state.local.is_scoped = local_snap.get<bool>(g_offsets.C_CSPlayerPawn.m_bIsScoped);

            uintptr_t local_scene = local_snap.get<uintptr_t>(
                g_offsets.C_BaseEntity.m_pGameSceneNode);
            if (local_scene) {
                // --- 1 RPM: local scene origin ---
                Vec3 origin = g_memory.read<Vec3>(
                    local_scene + g_offsets.CGameSceneNode.m_vecAbsOrigin);
                state.local.x = origin.x;
                state.local.y = origin.y;
            }
            state.local.yaw = atan2f(state.view_matrix.m[0][1],
                                     state.view_matrix.m[0][0])
                              * 180.0f / 3.14159265f - 90.0f;
        }

        // --- 1 RPM: entity list ptr ---
        state.entity_list = g_memory.read<uintptr_t>(
            g_memory.get_client_base() + g_offsets.client.dwEntityList);
        if (!state.entity_list) return state;

        // --- 1 RPM: first page ptr ---
        uintptr_t first_page = g_memory.read<uintptr_t>(
            state.entity_list + EntityList::PAGE_HEADER);
        if (!first_page) return state;

        // ---------------------------------------------------------------
        // KEY OPTIMIZATION: read the entire entity page in ONE RPM call.
        // Previously: 63 individual read<uintptr_t> calls = 63 syscalls.
        // Now: 1 bulk read of 7056 bytes = 1 syscall.
        // Each slot is 112 bytes (ENTRY_STRIDE); we need the pointer at
        // offset 0 within each slot.
        // ---------------------------------------------------------------
        static constexpr size_t PAGE_BUF_SIZE =
            EntityList::ENTRY_STRIDE * EntityList::MAX_PLAYERS;
        static uint8_t page_buf[PAGE_BUF_SIZE];

        if (!g_memory.read_raw(first_page, page_buf, PAGE_BUF_SIZE))
            return state;

        for (int i = 1; i < EntityList::MAX_PLAYERS; i++) {
            uintptr_t controller;
            memcpy(&controller,
                   page_buf + EntityList::ENTRY_STRIDE * (i & EntityList::INDEX_MASK),
                   sizeof(uintptr_t));
            if (!controller) continue;

            read_player(state, controller, i, screen_w, screen_h);
        }

        return state;
    }

private:
    CBoneData bone_buf[MAX_BONE];

    void read_weapon(uintptr_t pawn, char* out_name, size_t max_len,
                     uint16_t& out_def_index) {
        out_name[0] = 0;
        out_def_index = 0;

        // --- 1 RPM: weapon ptr ---
        uintptr_t weapon = g_memory.read<uintptr_t>(
            pawn + g_offsets.C_CSPlayerPawnBase.m_pClippingWeapon);
        if (!weapon) return;

        // --- 1 RPM: def index ---
        uint16_t def_index = g_memory.read<uint16_t>(
            weapon + g_offsets.C_EconEntity.m_AttributeManager
                   + g_offsets.C_AttributeContainer.m_Item
                   + g_offsets.C_EconItemView.m_iItemDefinitionIndex);

        if (def_index == 0) return;
        out_def_index = def_index;

        const WeaponInfo* info = lookup_weapon(def_index);
        if (info)
            snprintf(out_name, max_len, "%s", info->name);
        else
            snprintf(out_name, max_len, "Weapon %d", def_index);
    }

    // Signature changed: takes controller ptr directly (extracted from page_buf
    // by the caller) instead of re-reading it from first_page.
    void read_player(FrameState& state, uintptr_t controller, int i,
                     int screen_w, int screen_h) {
        char name[128]{};
        read_player_name(controller, name, sizeof(name));

        uint32_t pawn_handle = get_pawn_handle(controller);
        if (!pawn_handle) return;

        uintptr_t pawn = EntityList::resolve_handle(state.entity_list, pawn_handle);
        if (!pawn || pawn == state.local.pawn) return;

        // ---------------------------------------------------------------
        // KEY OPTIMIZATION: bulk-read pawn in one RPM call.
        // Previously: separate RPM calls for health, team, scene_node, origin.
        // Now: 1 RPM call, then local memcpy for each field.
        // ---------------------------------------------------------------
        PawnSnapshot snap{};
        if (!g_memory.read_raw(pawn, snap.buf, PawnSnapshot::SIZE)) return;

        int health = snap.get<int>(g_offsets.C_BaseEntity.m_iHealth);
        if (health <= 0) return;

        int team = snap.get<int>(g_offsets.C_BaseEntity.m_iTeamNum);

        uintptr_t scene_node = snap.get<uintptr_t>(g_offsets.C_BaseEntity.m_pGameSceneNode);
        if (!scene_node) return;

        // --- 1 RPM: scene origin (external ptr, can't batch with pawn) ---
        Vec3 origin = g_memory.read<Vec3>(
            scene_node + g_offsets.CGameSceneNode.m_vecAbsOrigin);

        // --- 1 RPM: bone array ptr (inside scene_node) ---
        uintptr_t bone_array = g_memory.read<uintptr_t>(
            scene_node + g_offsets.CSkeletonInstance.m_modelState + 0x80);
        if (!bone_array) return;

        // --- 1 RPM: all bones in one read ---
        if (!g_memory.read_raw(bone_array, bone_buf, sizeof(bone_buf))) return;

        auto& player = state.players[i];
        player.valid = true;
        player.team = team;
        player.health = health;
        player.origin = origin;
        memcpy(player.name, name, 128);

        read_weapon(pawn, player.weapon, sizeof(player.weapon),
                    player.weapon_def_index);

        for (int b = 0; b < MAX_BONE; b++)
            player.visible[b] = w2s_depth(
                bone_buf[b].pos, state.view_matrix,
                screen_w, screen_h,
                player.screens[b], player.depths[b]);

        auto& rp = state.radar_players[i];
        rp = {origin.x, origin.y, origin.z, team, health, true, {}};
        memcpy(rp.name, name, 128);
    }
};