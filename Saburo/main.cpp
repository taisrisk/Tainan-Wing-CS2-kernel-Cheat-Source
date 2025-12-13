// Orchestrated entry that uses multiple small helpers
#include <atomic>
#include <windows.h>
#include "src/Helpers/bootstrap.hpp"
#include "src/Helpers/offsets_init.hpp"
#include "src/Helpers/esp_setup.hpp"
#include "src/Helpers/features_init.hpp"
#include "src/Helpers/game_state.hpp"
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/loop_runner.hpp"

int main() {
    // 1) Logger and version
    Bootstrap::InitializeLogger();
    VersionManager::VersionConfig cfg;
    if (!Bootstrap::VersionCheckAndEnsureFiles(cfg)) return 1;

    // 2) Driver connect
    driver::DriverHandle drv;
    if (!Bootstrap::ConnectOrLoadDriver(drv, cfg)) return 1;

    // 3) Offsets from headers with version-aware cache
    if (!OffsetsInit::Initialize(cfg.version)) return 1;
    // Dump resolved offsets for diagnostics
    OffsetsManager::PrintOffsets();

    // 4) Wait for CS2 and attach
    std::atomic<bool> cs2Running(true), shutdown(false);
    DWORD pid = Bootstrap::WaitForProcess(L"cs2.exe");
    Bootstrap::StartProcessMonitor(cs2Running, shutdown, pid);
    if (!Bootstrap::AttachToProcess(drv, pid)) return 1;

    // 5) Find client.dll
    std::uintptr_t client = 0;
    if (!Bootstrap::WaitForClientDll(drv, pid, client)) return 1;

    // 6) ESP setup
    ImGuiESP esp(drv, client);
    if (!ESPSetup::StartOverlay(drv, client, esp)) {
        ConsoleColor::PrintWarning("Continuing without ESP overlay");
    }
    // Show blue border while in lobby/waiting
    esp.setLobbyMode(true);

    // 7) Kernel mouse
    ConsoleColor::PrintInfo("Initializing aimbot...");
    if (!KernelMouse::Initialize(L"\\\\.\\airway_radio")) {
        ConsoleColor::PrintWarning("Aimbot disabled - camera control unavailable");
    } else {
        ConsoleColor::PrintSuccess("Aimbot ready");
    }

    // 8) Features and toggles
    Features features(drv, client);
    features.bhop.setTargetProcessId(pid);
    features.bhop.setJumpDelayMs(10);
    Toggles toggles = FeaturesInit::LoadToggles();
    FeaturesInit::ApplyToggles(features, esp, toggles);

    // 9) Wait for in-game state
    if (!GameState::WaitForInGame(drv, client, esp, cs2Running)) return 0;
    // Switch overlay to in-game visuals (red border)
    esp.setLobbyMode(false);
    // Prepare console for clean status panel (no previous logs, no cursor)
    ConsoleColor::ClearScreen();
    ConsoleColor::HideCursor();
    // Silence console logs while rendering status panel to avoid overlap
    ConsoleColor::SetQuiet(true);

    // 10) Run main loop
    LoopRunner::Run(drv, client, esp, features, toggles, cs2Running);
    KernelMouse::Shutdown();
    return 0;
}
