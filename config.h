#pragma once
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include "settings.h"

class Config {
public:
    static bool save(const std::string& path) {
        std::ofstream f(path);
        if (!f) return false;

        f << "[ESP]\n";
        write(f, "master_switch", g_settings.master_switch);
        write(f, "esp_enabled", g_settings.esp_enabled);
        write(f, "draw_box", g_settings.draw_box);
        write(f, "draw_healthbar", g_settings.draw_healthbar);
        write(f, "draw_health_text", g_settings.draw_health_text);
        write(f, "draw_name", g_settings.draw_name);
        write(f, "draw_teammates", g_settings.draw_teammates);
        write(f, "draw_skeleton_wire", g_settings.draw_skeleton_wire);
        write(f, "chams_style", g_settings.chams_style);

        f << "\n[Spectators]\n";
        write(f, "draw_spectators", g_settings.draw_spectators);

        f << "\n[Radar]\n";
        write(f, "draw_radar", g_settings.draw_radar);
        write(f, "radar_circle", g_settings.radar_circle);
        write(f, "radar_rotate", g_settings.radar_rotate);
        write(f, "radar_rings", g_settings.radar_rings);
        write(f, "radar_names", g_settings.radar_names);
        write(f, "radar_size", g_settings.radar_size);
        write(f, "radar_range", g_settings.radar_range);
        write(f, "radar_bg_alpha", g_settings.radar_bg_alpha);
        write(f, "radar_x", g_settings.radar_x);
        write(f, "radar_y", g_settings.radar_y);

        f << "\n[Colors]\n";
        write_arr(f, "enemy_fill", g_settings.enemy_fill, 4);
        write_arr(f, "enemy_outline", g_settings.enemy_outline, 4);
        write_arr(f, "enemy_glow", g_settings.enemy_glow, 4);
        write_arr(f, "team_fill", g_settings.team_fill, 4);
        write_arr(f, "team_outline", g_settings.team_outline, 4);
        write_arr(f, "team_glow", g_settings.team_glow, 4);

        f << "\n[Body]\n";
        write(f, "body_width_scale", g_settings.body_width_scale);
        write(f, "head_radius", g_settings.head_radius);
        write(f, "depth_scale", g_settings.depth_scale);
        write(f, "glow_expand_outer", g_settings.glow_expand_outer);
        write(f, "glow_expand_inner", g_settings.glow_expand_inner);
        write_arr(f, "limb_width_a", g_settings.limb_width_a, 15);
        write_arr(f, "limb_width_b", g_settings.limb_width_b, 15);

        f << "\n[Misc]\n";
        write(f, "target_fps", g_settings.target_fps);
        write(f, "box_smoothing", g_settings.box_smoothing);
        write(f, "esp_font_size", g_settings.esp_font_size);

        f << "\n[Box]\n";
        write(f, "box_style", g_settings.box_style);
        write(f, "box_thickness", g_settings.box_thickness);
        write(f, "box_padding_x", g_settings.box_padding_x);
        write(f, "box_padding_y", g_settings.box_padding_y);
        write(f, "box_corner_pct", g_settings.box_corner_pct);

        f << "\n[Crosshair]\n";
        write(f, "crosshair_enabled", g_settings.crosshair_enabled);
        write(f, "crosshair_shape", g_settings.crosshair_shape);
        write(f, "crosshair_size", g_settings.crosshair_size);
        write(f, "crosshair_gap", g_settings.crosshair_gap);
        write(f, "crosshair_thickness", g_settings.crosshair_thickness);
        write_arr(f, "crosshair_color", g_settings.crosshair_color, 4);
        write(f, "crosshair_outline", g_settings.crosshair_outline);
        write(f, "crosshair_outline_thickness", g_settings.crosshair_outline_thickness);
        write_arr(f, "crosshair_outline_color", g_settings.crosshair_outline_color, 4);
        write(f, "crosshair_dot", g_settings.crosshair_dot);
        write(f, "crosshair_dot_size", g_settings.crosshair_dot_size);

        f.close();
        return true;
    }

    static bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;

        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '[' || line[0] == '#' || line[0] == ';')
                continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));
            kv[key] = val;
        }

        read(kv, "master_switch", g_settings.master_switch);
        read(kv, "esp_enabled", g_settings.esp_enabled);
        read(kv, "draw_box", g_settings.draw_box);
        read(kv, "draw_healthbar", g_settings.draw_healthbar);
        read(kv, "draw_health_text", g_settings.draw_health_text);
        read(kv, "draw_name", g_settings.draw_name);
        read(kv, "draw_teammates", g_settings.draw_teammates);
        read(kv, "draw_skeleton_wire", g_settings.draw_skeleton_wire);
        read(kv, "chams_style", g_settings.chams_style);
        read(kv, "draw_spectators", g_settings.draw_spectators);
        read(kv, "draw_radar", g_settings.draw_radar);
        read(kv, "radar_circle", g_settings.radar_circle);
        read(kv, "radar_rotate", g_settings.radar_rotate);
        read(kv, "radar_rings", g_settings.radar_rings);
        read(kv, "radar_names", g_settings.radar_names);
        read(kv, "radar_size", g_settings.radar_size);
        read(kv, "radar_range", g_settings.radar_range);
        read(kv, "radar_bg_alpha", g_settings.radar_bg_alpha);
        read(kv, "radar_x", g_settings.radar_x);
        read(kv, "radar_y", g_settings.radar_y);
        read_arr(kv, "enemy_fill", g_settings.enemy_fill, 4);
        read_arr(kv, "enemy_outline", g_settings.enemy_outline, 4);
        read_arr(kv, "enemy_glow", g_settings.enemy_glow, 4);
        read_arr(kv, "team_fill", g_settings.team_fill, 4);
        read_arr(kv, "team_outline", g_settings.team_outline, 4);
        read_arr(kv, "team_glow", g_settings.team_glow, 4);
        read(kv, "body_width_scale", g_settings.body_width_scale);
        read(kv, "head_radius", g_settings.head_radius);
        read(kv, "depth_scale", g_settings.depth_scale);
        read(kv, "glow_expand_outer", g_settings.glow_expand_outer);
        read(kv, "glow_expand_inner", g_settings.glow_expand_inner);
        read_arr(kv, "limb_width_a", g_settings.limb_width_a, 15);
        read_arr(kv, "limb_width_b", g_settings.limb_width_b, 15);
        read(kv, "target_fps", g_settings.target_fps);
        read(kv, "box_smoothing", g_settings.box_smoothing);
        read(kv, "esp_font_size", g_settings.esp_font_size);
        read(kv, "box_style", g_settings.box_style);
        read(kv, "box_thickness", g_settings.box_thickness);
        read(kv, "box_padding_x", g_settings.box_padding_x);
        read(kv, "box_padding_y", g_settings.box_padding_y);
        read(kv, "box_corner_pct", g_settings.box_corner_pct);
        read(kv, "crosshair_enabled", g_settings.crosshair_enabled);
        read(kv, "crosshair_shape", g_settings.crosshair_shape);
        read(kv, "crosshair_size", g_settings.crosshair_size);
        read(kv, "crosshair_gap", g_settings.crosshair_gap);
        read(kv, "crosshair_thickness", g_settings.crosshair_thickness);
        read_arr(kv, "crosshair_color", g_settings.crosshair_color, 4);
        read(kv, "crosshair_outline", g_settings.crosshair_outline);
        read(kv, "crosshair_outline_thickness", g_settings.crosshair_outline_thickness);
        read_arr(kv, "crosshair_outline_color", g_settings.crosshair_outline_color, 4);
        read(kv, "crosshair_dot", g_settings.crosshair_dot);
        read(kv, "crosshair_dot_size", g_settings.crosshair_dot_size);

        return true;
    }

private:
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }

    static void write(std::ofstream& f, const char* key, bool v) {
        f << key << " = " << (v ? 1 : 0) << "\n";
    }
    static void write(std::ofstream& f, const char* key, int v) {
        f << key << " = " << v << "\n";
    }
    static void write(std::ofstream& f, const char* key, float v) {
        f << key << " = " << v << "\n";
    }
    static void write_arr(std::ofstream& f, const char* key, const float* v, int n) {
        f << key << " = ";
        for (int i = 0; i < n; i++) {
            if (i) f << ",";
            f << v[i];
        }
        f << "\n";
    }

    static void read(const std::unordered_map<std::string, std::string>& kv,
                     const char* key, bool& v) {
        auto it = kv.find(key);
        if (it != kv.end()) v = (std::stoi(it->second) != 0);
    }
    static void read(const std::unordered_map<std::string, std::string>& kv,
                     const char* key, int& v) {
        auto it = kv.find(key);
        if (it != kv.end()) v = std::stoi(it->second);
    }
    static void read(const std::unordered_map<std::string, std::string>& kv,
                     const char* key, float& v) {
        auto it = kv.find(key);
        if (it != kv.end()) v = std::stof(it->second);
    }
    static void read_arr(const std::unordered_map<std::string, std::string>& kv,
                         const char* key, float* v, int n) {
        auto it = kv.find(key);
        if (it == kv.end()) return;
        std::istringstream ss(it->second);
        std::string token;
        for (int i = 0; i < n && std::getline(ss, token, ','); i++)
            v[i] = std::stof(token);
    }
};