#include "game_state.hpp"
#include "../Offsets/offset_manager.hpp"
#include "console_colors.hpp"
#include "../Features/entity.hpp"
#include <thread>
#include <chrono>

namespace {
static std::uintptr_t get_entity_from_handle(driver::DriverHandle& drv, std::uintptr_t client, uint32_t handle) {
    if (handle == 0) return 0;
    const uint32_t index = handle & 0x7FFF;
    std::uintptr_t entity_list = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwEntityList);
    if (entity_list == 0) return 0;
    const uint32_t chunk_index = index >> 9;
    std::uintptr_t chunk_pointer_address = entity_list + (0x8 * chunk_index) + 0x10;
    std::uintptr_t list_entry = drv.read<std::uintptr_t>(chunk_pointer_address);
    if (list_entry == 0) return 0;
    const uint32_t entity_index = index & 0x1FF;
    std::uintptr_t entity_address = list_entry + (0x70 * entity_index);
    return drv.read<std::uintptr_t>(entity_address);
}
}

namespace GameState {

bool WaitForInGame(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& espRenderer,
                   std::atomic<bool>& cs2Running) {
    using namespace ConsoleColor;
    const auto& offsets = OffsetsManager::Get();

    // quick check
    {
        std::uintptr_t quickCtrl = drv.read<std::uintptr_t>(client + offsets.dwLocalPlayerController);
        if (quickCtrl) {
            uint32_t h = drv.read<uint32_t>(quickCtrl + offsets.m_hPlayerPawn);
            if (h) {
                auto pawn = get_entity_from_handle(drv, client, h);
                if (pawn) {
                    int32_t hp = drv.read<int32_t>(pawn + offsets.m_iHealth);
                    uint8_t team = drv.read<uint8_t>(pawn + offsets.m_iTeamNum);
                    if (hp > 0 && hp <= 150 && (team == 2 || team == 3)) {
                        PrintSuccess("Already in-game - ready to scan entities");
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        return true;
                    }
                }
            }
        }
    }

    ClearScreen();
    PrintLogo(false);
    std::cout << "\n";
    PrintInfo("Waiting for in-game state...");
    std::cout << "\n  Please JOIN A GAME\n    - Casual/Competitive\n    - Deathmatch\n    - Practice/Workshop\n\n";

    int consecutiveValidChecks = 0;
    const int requiredValidChecks = 3;
    while (true) {
        if (!cs2Running.load()) return false;

        // entity list present
        auto entityList = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwEntityList);
        if (entityList == 0) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }

        // view matrix looks valid
        auto vmAddr = client + OffsetsManager::Get().dwViewMatrix;
        float vm[16]{}; drv.read_memory(reinterpret_cast<void*>(vmAddr), vm, sizeof(vm));
        bool nonzero = false; for (float f : vm) { if (f != 0.f) { nonzero = true; break; } }
        if (!nonzero) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }

        // local pawn valid and alive on a valid team
        auto ctrl = drv.read<std::uintptr_t>(client + offsets.dwLocalPlayerController);
        if (!ctrl) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        uint32_t hPawn = drv.read<uint32_t>(ctrl + offsets.m_hPlayerPawn);
        if (!hPawn) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        auto pawn = get_entity_from_handle(drv, client, hPawn);
        if (!pawn) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        int32_t hp = drv.read<int32_t>(pawn + offsets.m_iHealth);
        uint8_t team = drv.read<uint8_t>(pawn + offsets.m_iTeamNum);
        if (hp <= 0 || hp > 150 || (team != 2 && team != 3)) { consecutiveValidChecks = 0; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }

        if (++consecutiveValidChecks >= requiredValidChecks) {
            PrintSuccess("In-game state detected!");
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

}
