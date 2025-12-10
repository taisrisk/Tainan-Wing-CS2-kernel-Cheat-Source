#pragma once

#include <thread>
#include <chrono>
#include <cstdint>
#include <iostream>
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/logger.hpp"
#include "src/Offsets/offset_manager.hpp"
#include "src/Offsets/offsets.hpp"
#include "src/Features/ImGuiESP.hpp"
#include "app_state.hpp"
#include "entity_utils.hpp"

namespace GameStateWaiter {
    inline bool WaitForInGameState(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& espRenderer) {
        using namespace ConsoleColor;
        const auto& offsets = OffsetsManager::Get();

        // Fast path: controller -> pawn
        std::uintptr_t quickCheckController = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
        if (quickCheckController != 0) {
            uint32_t quickCheckHandle = 0;
            try { quickCheckHandle = drv.read<uint32_t>(quickCheckController + offsets.m_hPlayerPawn); } catch (...) { quickCheckHandle = 0; }
            if (quickCheckHandle != 0) {
                std::uintptr_t quickCheckPawn = EntityUtils::get_entity_from_handle(drv, client, quickCheckHandle);
                if (quickCheckPawn != 0) {
                    int32_t quickCheckHealth = drv.read<int32_t>(quickCheckPawn + offsets.m_iHealth);
                    uint8_t quickCheckTeam = drv.read<uint8_t>(quickCheckPawn + offsets.m_iTeamNum);
                    if (quickCheckHealth > 0 && quickCheckHealth <= 150 && (quickCheckTeam == 2 || quickCheckTeam == 3)) {
                        PrintSuccess("Already in-game - ready to scan entities");
                        Logger::LogSuccess("Already in-game - ready to scan entities");
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        return true;
                    }
                }
            }
        }

        // Direct pawn fallback
        std::uintptr_t directPawn = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
        if (directPawn != 0) {
            try {
                int32_t health = drv.read<int32_t>(directPawn + offsets.m_iHealth);
                uint8_t team = drv.read<uint8_t>(directPawn + offsets.m_iTeamNum);
                if (health > 0 && health <= 150 && (team == 2 || team == 3)) {
                    PrintSuccess("Already in-game (direct pawn) - ready to scan entities");
                    Logger::LogSuccess("Already in-game (direct pawn) - ready to scan entities");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    return true;
                }
            } catch (...) {}
        }

        // Lobby screen
        ClearScreen();
        PrintLogo(false);
        std::cout << "\n";
        PrintInfo("Waiting for in-game state...");
        Logger::LogInfo("Waiting for in-game state...");
        std::cout << "\n";
        SetColor(ConsoleColor::YELLOW);
        std::cout << "  Please JOIN A GAME:\n";
        Reset();
        std::cout << "    - Casual/Competitive Match\n";
        std::cout << "    - Deathmatch\n";
        std::cout << "    - Practice/Workshop Map\n\n";
        SetColor(ConsoleColor::DARK_GRAY);
        std::cout << "  (Press ESC to cancel if CS2 closed)\n\n";
        Reset();

        int consecutiveValidChecks = 0;
        const int requiredValidChecks = 3;
        while (true) {
            if (!g_cs2ProcessRunning.load()) {
                PrintWarning("CS2 process closed while waiting for game!");
                Logger::LogWarning("CS2 process closed during lobby wait");
                return false;
            }

            std::uintptr_t lobbyLocalController = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
            if (lobbyLocalController != 0) {
                uint32_t lobbyHandle = drv.read<uint32_t>(lobbyLocalController + offsets.m_hPlayerPawn);
                if (lobbyHandle != 0) {
                    std::uintptr_t lobbyLocalPawn = EntityUtils::get_entity_from_handle(drv, client, lobbyHandle);
                    if (lobbyLocalPawn != 0) {
                        EntityManager lobbyEntities;  // Empty
                        espRenderer.updateEntities(lobbyEntities, lobbyLocalPawn);
                    }
                }
            } else {
                std::uintptr_t pawn = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
                if (pawn != 0) {
                    EntityManager lobbyEntities;
                    espRenderer.updateEntities(lobbyEntities, pawn);
                }
            }

            bool allChecksPass = true;
            std::uintptr_t entityList = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwEntityList);
            if (entityList == 0) allChecksPass = false;

            if (allChecksPass) {
                std::uintptr_t viewMatrixAddr = client + cs2_dumper::offsets::client_dll::dwViewMatrix;
                float viewMatrix[16] = {0};
                drv.read_memory(reinterpret_cast<PVOID>(viewMatrixAddr), viewMatrix, sizeof(viewMatrix));
                bool hasNonZero = false;
                for (int i = 0; i < 16; i++) { if (viewMatrix[i] != 0.0f) { hasNonZero = true; break; } }
                if (!hasNonZero) allChecksPass = false;
            }

            if (allChecksPass) {
                bool localValid = false;
                std::uintptr_t localController = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
                if (localController != 0) {
                    uint32_t pawnHandle = drv.read<uint32_t>(localController + offsets.m_hPlayerPawn);
                    if (pawnHandle != 0) {
                        std::uintptr_t localPlayerPawn = EntityUtils::get_entity_from_handle(drv, client, pawnHandle);
                        if (localPlayerPawn != 0) {
                            int32_t health = drv.read<int32_t>(localPlayerPawn + offsets.m_iHealth);
                            uint8_t team = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);
                            if (health > 0 && health <= 150 && (team == 2 || team == 3)) localValid = true;
                        }
                    }
                }
                if (!localValid) {
                    std::uintptr_t directLocalPawn = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
                    if (directLocalPawn != 0) {
                        int32_t health = drv.read<int32_t>(directLocalPawn + offsets.m_iHealth);
                        uint8_t team = drv.read<uint8_t>(directLocalPawn + offsets.m_iTeamNum);
                        if (health > 0 && health <= 150 && (team == 2 || team == 3)) localValid = true;
                    }
                }
                if (!localValid) allChecksPass = false;
            }

            if (allChecksPass) {
                consecutiveValidChecks++;
                if (consecutiveValidChecks >= requiredValidChecks) {
                    PrintSuccess("In-game state detected!");
                    Logger::LogSuccess("In-game state detected!");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    return true;
                }
            } else {
                consecutiveValidChecks = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
