#include "loop_runner.hpp"
#include "../Offsets/offset_manager.hpp"
#include "../Features/entity_enumerator.hpp"
#include "console_colors.hpp"
#include "toggle_manager.hpp"
#include <array>
#include <Windows.h>
#include <conio.h>

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

namespace LoopRunner {

void Run(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& esp,
         Features& f, Toggles& t, std::atomic<bool>& cs2Running) {
    // Ensure a clean console region for the status panel
    ConsoleColor::ClearScreen();
    // Get local pawn
    std::uintptr_t localPawn = 0;
    for (int i=0;i<50 && localPawn==0;i++) {
        auto ctrl = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
        if (!ctrl) { Sleep(200); continue; }
        uint32_t h = drv.read<uint32_t>(ctrl + OffsetsManager::Get().m_hPlayerPawn);
        if (!h) { Sleep(200); continue; }
        localPawn = get_entity_from_handle(drv, client, h);
        if (!localPawn) Sleep(200);
    }
    if (!localPawn) return;

    f.triggerbot.setLocalPlayerPawn(localPawn);
    f.bhop.setLocalPlayerPawn(localPawn);
    f.aimbot.setLocalPlayerPawn(localPawn);
    // We are in-game at this point. Ensure ESP leaves lobby mode.
    esp.setLobbyMode(false);

    // Build a toggle manager to handle hotkeys and keep settings in sync
    ToggleManager toggleManager(
        f.triggerbot,
        f.bhop,
        f.aimbot,
        f.chams,
        f.boneEsp,
        f.thirdPerson,
        f.visibilityChecker,
        f.smoothAim,
        f.recoilComp,
        esp,
        f.silentAim,
        f.entityPredictor,
        t.triggerbotEnabled,
        t.bhopEnabled,
        t.aimbotEnabled,
        t.consoleDebugEnabled,
        t.teamCheckEnabled,
        t.headAngleLineEnabled,
        t.snaplinesEnabled,
        t.distanceESPEnabled,
        t.snaplinesWallCheckEnabled,
        t.chamsEnabled,
        t.boneESPEnabled,
        t.silentAimEnabled,
        t.thirdPersonEnabled,
        t.recoilCompEnabled,
        t.smoothAimEnabled,
        t.entityPredictorEnabled,
        t.visibilityCheckEnabled
    );
    // Ensure everything matches the current toggles once
    toggleManager.applyAll();

    EntityEnumerator enumerator(drv, client);
    EntityManager entities;

    int frame=0;
    static std::array<bool,256> prev{};
    static HWND hConsoleWnd = GetConsoleWindow();
    auto on_press = [&](int vk, int keyToHandle){
        SHORT s = GetAsyncKeyState(vk);
        bool down = (s & 0x8000) != 0;
        bool was = prev[static_cast<unsigned char>(vk)];
        prev[static_cast<unsigned char>(vk)] = down;
        if (down && !was) toggleManager.handleKey(keyToHandle);
    };

    int noPlayersFrameCount = 0;
    static constexpr int QUICK_RESCAN_THRESHOLD = 30;
    static constexpr int FORCE_RESCAN_THRESHOLD = 120;

    while (cs2Running.load()) {
        // Only process toggle hotkeys when our console window is focused
        bool appFocused = (GetForegroundWindow() == hConsoleWnd);

        enumerator.update_entities(entities);
        esp.updateEntities(entities, localPawn);

        const bool noPlayers = entities.all_entities.empty();
        if (noPlayers) {
            noPlayersFrameCount++;
            if (noPlayersFrameCount == QUICK_RESCAN_THRESHOLD) {
                enumerator.update_entities(entities);
            }
            if (noPlayersFrameCount == FORCE_RESCAN_THRESHOLD) {
                enumerator.force_rescan(entities, localPawn);
            }
        } else {
            noPlayersFrameCount = 0;
        }

        // Flip to in-game visuals once entities are populated
        if (!noPlayers) {
            esp.setLobbyMode(false);
        }

        if (t.chamsEnabled) f.chams.update(entities, localPawn);
        if (t.triggerbotEnabled) { f.triggerbot.updateEnemyCache(entities); f.triggerbot.update(); }
        if (t.bhopEnabled) f.bhop.update();
        if (t.aimbotEnabled) f.aimbot.update(entities);
        if (t.thirdPersonEnabled) f.thirdPerson.update(localPawn);
        if (t.silentAimEnabled && t.thirdPersonEnabled) f.silentAim.update();

        // Hotkeys (edge-triggered) only when app focused; otherwise clear edge history
        if (appFocused) {
            on_press('0','0');
            on_press('1','1');
            on_press('2','2');
            on_press('3','3');
            on_press('4','4');
            on_press('5','5');
            on_press('6','6');
            on_press('7','7');
            on_press('8','8');
            on_press('9','9');
            on_press('Q','q');
            on_press('W','w');
            on_press('E','e');
            on_press('R','r');
            on_press('T','t');
            on_press('Y','y');
            on_press('U','u');
        } else {
            // Reset so no stale edges toggle when refocusing
            prev.fill(false);
        }

        if ((frame++ % 60) == 0) {
            ConsoleColor::PrintUnifiedPanel(
                frame,
                static_cast<int>(entities.all_entities.size()),
                static_cast<int>(entities.enemy_entities.size()),
                static_cast<int>(entities.teammate_entities.size()),
                t.aimbotEnabled,
                t.triggerbotEnabled,
                t.bhopEnabled,
                t.headAngleLineEnabled,
                t.snaplinesEnabled,
                t.distanceESPEnabled,
                t.snaplinesWallCheckEnabled,
                t.teamCheckEnabled,
                t.chamsEnabled,
                t.boneESPEnabled,
                t.silentAimEnabled,
                t.thirdPersonEnabled,
                t.recoilCompEnabled,
                t.smoothAimEnabled,
                t.entityPredictorEnabled,
                t.visibilityCheckEnabled,
                t.consoleDebugEnabled,
                true
            );
        }
        Sleep(16);
    }
}

}
