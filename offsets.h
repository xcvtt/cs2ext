#pragma once
#include <fstream>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

struct Offsets {
    struct { uint32_t dwEntityList, dwViewMatrix, dwLocalPlayerPawn, dwLocalPlayerController; } client;
    struct { uint32_t m_iTeamNum, m_pGameSceneNode, m_iHealth; } C_BaseEntity;
    struct { uint32_t m_vecAbsOrigin; } CGameSceneNode;
    struct { uint32_t m_modelState; } CSkeletonInstance;
    struct {
        uint32_t m_hPlayerPawn;
        uint32_t m_hPawn;
        uint32_t m_sSanitizedPlayerName;
    } CCSPlayerController;
    struct { uint32_t m_pObserverServices; } C_BasePlayerPawn;
    struct { uint32_t m_hObserverTarget; } CPlayer_ObserverServices;
    struct { uint32_t m_bIsScoped; } C_CSPlayerPawn;
    struct { uint32_t m_vecViewOffset; } C_BaseModelEntity;

    bool load(const std::string& offsets_path, const std::string& client_dll_path) {
        try {
            nlohmann::json oj, cj;
            { std::ifstream f(offsets_path); if (!f) return false; f >> oj; }
            { std::ifstream f(client_dll_path); if (!f) return false; f >> cj; }

            auto& cl = oj["client.dll"];
            client.dwEntityList = cl["dwEntityList"];
            client.dwViewMatrix = cl["dwViewMatrix"];
            client.dwLocalPlayerPawn = cl["dwLocalPlayerPawn"];
            client.dwLocalPlayerController = cl["dwLocalPlayerController"];

            auto& cs = cj["client.dll"]["classes"];
            C_BaseEntity.m_iTeamNum = cs["C_BaseEntity"]["fields"]["m_iTeamNum"];
            C_BaseEntity.m_pGameSceneNode = cs["C_BaseEntity"]["fields"]["m_pGameSceneNode"];
            C_BaseEntity.m_iHealth = cs["C_BaseEntity"]["fields"]["m_iHealth"];
            CGameSceneNode.m_vecAbsOrigin = cs["CGameSceneNode"]["fields"]["m_vecAbsOrigin"];
            CSkeletonInstance.m_modelState = cs["CSkeletonInstance"]["fields"]["m_modelState"];

            CCSPlayerController.m_hPlayerPawn = cs["CCSPlayerController"]["fields"]["m_hPlayerPawn"];
            CCSPlayerController.m_sSanitizedPlayerName = cs["CCSPlayerController"]["fields"]["m_sSanitizedPlayerName"];

            // m_hPawn is on CBasePlayerController, not CCSPlayerController
            CCSPlayerController.m_hPawn = cs["CBasePlayerController"]["fields"]["m_hPawn"];

            C_BasePlayerPawn.m_pObserverServices = cs["C_BasePlayerPawn"]["fields"]["m_pObserverServices"];
            CPlayer_ObserverServices.m_hObserverTarget = cs["CPlayer_ObserverServices"]["fields"]["m_hObserverTarget"];

            C_CSPlayerPawn.m_bIsScoped = cs["C_CSPlayerPawn"]["fields"]["m_bIsScoped"];
            C_BaseModelEntity.m_vecViewOffset = cs["C_BaseModelEntity"]["fields"]["m_vecViewOffset"];

            printf("[offsets] m_hPlayerPawn=0x%X m_hPawn=0x%X m_pObserverServices=0x%X m_hObserverTarget=0x%X\n",
                CCSPlayerController.m_hPlayerPawn, CCSPlayerController.m_hPawn,
                C_BasePlayerPawn.m_pObserverServices, CPlayer_ObserverServices.m_hObserverTarget);

            return true;
        }
        catch (const std::exception& e) {
            printf("Offset error: %s\n", e.what());
            return false;
        }
    }
};

inline Offsets g_offsets;