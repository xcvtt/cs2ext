#pragma once
#include <atomic>
#include <cstdio>
#include <thread>

#include "entity_reader.h"
#include "overlay.h"
#include "settings.h"
#include "shared_state.h"
#include "types.h"
#include "utils.h"
#include "input/input.h"
#include "systems/bvh.h"

static std::thread g_aimbot_thread;
static std::atomic<bool> g_aimbot_running{ false };

static void aimbot_thread_func()
{
    printf("[+] Aimbot thread started (tid: %lu)\n", GetCurrentThreadId());

    static double tick_fps = 500.0f;

    while (g_aimbot_running.load(std::memory_order_relaxed))
    {
        auto tick_start = std::chrono::high_resolution_clock::now();

        if (!g_settings.master_switch || !g_settings.aimbot_enabled)
        {
            limit_frame(tick_start, 20.0);
            continue;
        }

        if (!(GetAsyncKeyState(g_settings.key_aimbot) & 0x8000))
        {
            limit_frame(tick_start, tick_fps);
            continue;
        }

        // ─── Grab latest frame (no memory reads!) ─────────
        AimbotFrame frame = g_aimbot_data.snapshot();

        if (frame.local_pawn == 0 || frame.screen_w == 0)
        {
            limit_frame(tick_start, tick_fps);
            continue;
        }

        float center_x = static_cast<float>(frame.screen_w) / 2.0f;
        float center_y = static_cast<float>(frame.screen_h) / 2.0f;
        float fov_pixels = (static_cast<float>(g_settings.aimbot_fov) / 360.0f) * static_cast<float>(frame.screen_w);

        float best_score = FLT_MAX;
        float best_sx = 0, best_sy = 0;
        bool found = false;

        Vec3 eye_pos = {
            frame.local_x,
            frame.local_y,
            frame.local_z + 64.0f
        };

        for (int i = 1; i < EntityList::MAX_PLAYERS; i++)
        {
            const auto& t = frame.targets[i];
            if (!t.valid || t.health <= 0)  continue;
            if (t.team == frame.local_team) continue;

            ImVec2 screen;
            float depth;
            if (!w2s_depth(t.head_pos, frame.view_matrix,
                           frame.screen_w, frame.screen_h,
                           screen, depth))
                continue;

            float dx = screen.x - center_x;
            float dy = screen.y - center_y;
            float screen_dist = sqrtf(dx * dx + dy * dy);

            if (screen_dist > fov_pixels) continue;

            // Visibility check — skip if wall between us and head
            const auto trace = g_bvh.trace_ray(eye_pos, t.head_pos);
            if (trace.hit && trace.fraction <= 0.97f)
                continue;

            // World distance from local player to target head
            float wx = t.head_pos.x - frame.local_x;
            float wy = t.head_pos.y - frame.local_y;
            float wz = t.head_pos.z - frame.local_z;
            float world_dist = sqrtf(wx * wx + wy * wy + wz * wz);

            constexpr float screen_weight = 0.7f;
            constexpr float depth_weight  = 0.3f;
            constexpr float max_world_dist = 5000.0f;

            float norm_screen = screen_dist / fov_pixels;
            float norm_depth  = std::min(world_dist / max_world_dist, 1.0f);

            float score = (screen_weight * norm_screen)
                        + (depth_weight  * norm_depth);

            if (score < best_score)
            {
                best_score = score;
                best_sx = screen.x;
                best_sy = screen.y;
                found = true;
            }
        }

        // ─── Move mouse ───────────────────────────────────
        if (found)
        {
            float delta_x = best_sx - center_x;
            float delta_y = best_sy - center_y;

            constexpr float smooth = 3.0f;

            int move_x = static_cast<int>(delta_x / smooth);
            int move_y = static_cast<int>(delta_y / smooth);

            if (move_x == 0 && fabsf(delta_x) > 0.5f)
                move_x = (delta_x > 0) ? 1 : -1;
            if (move_y == 0 && fabsf(delta_y) > 0.5f)
                move_y = (delta_y > 0) ? 1 : -1;

            if (move_x != 0 || move_y != 0)
                g_input.inject_mouse(move_x, move_y, Input::move);
        }

        limit_frame(tick_start, tick_fps);
    }

    printf("[+] Aimbot thread exiting cleanly\n");
}

static void start_aimbot_thread()
{
    if (g_aimbot_running.load())
        return;

    g_aimbot_running.store(true);
    g_aimbot_thread = std::thread(aimbot_thread_func);
}

static void stop_aimbot_thread()
{
    if (!g_aimbot_running.load())
        return;

    g_aimbot_running.store(false);

    if (g_aimbot_thread.joinable())
    {
        g_aimbot_thread.join();
    }
}