#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>
#include <conio.h>
#include <atomic>

#include "src/Core/driver.hpp"
#include "src/Core/process.hpp"
#include "src/Offsets/offsets.hpp"
#include "src/Offsets/offset_manager.hpp"
#include "src/Features/entity.hpp"
#include "src/Features/entity_enumerator.hpp"
#include "src/Features/ImGuiESP.hpp"
#include "src/Features/triggerbot.hpp"
#include "src/Features/bhop.hpp"
#include "src/Features/Aimbot.hpp"
#include "src/Features/Chams.hpp"
#include "src/Features/HeadAngleLine.hpp"
#include "src/Features/BoneESP.hpp"
#include "src/Features/SilentAim.hpp"
#include "src/Features/ThirdPerson.hpp"
#include "src/Features/RecoilCompensation.hpp"
#include "src/Features/SmoothAim.hpp"
#include "src/Features/EntityPredictor.hpp"
#include "src/Features/VisibilityChecker.hpp"
#include "src/Helpers/console_logger.hpp"
#include "src/Helpers/kernel_mouse.hpp"
#include "src/Helpers/download_helper.hpp"
#include "src/Helpers/windows_security_helper.hpp"
#include "src/Helpers/driver_loader_helper.hpp"
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/version_manager.hpp"
#include "src/Helpers/logger.hpp"
#include "src/Helpers/settings_manager.hpp"
// New helpers to declutter main.cpp
#include "src/Helpers/ui_helper.hpp"
#include "src/Helpers/toggle_manager.hpp"
#include "src/Helpers/settings_helper.hpp"
// App orchestration helpers
#include "src/App/app_state.hpp"
#include "src/App/process_utils.hpp"
#include "src/App/driver_manager.hpp"
#include "src/App/entity_utils.hpp"
#include "src/App/game_state_waiter.hpp"

// App state moved to src/App/app_state.cpp

// Clear console without deleting all text (just scroll to new line)
void clear_line() {
    std::cout << "\r                                                                                                                                                                                                                 ";
}

// Set console cursor position
void set_cursor(int row) {
    COORD coord;
    coord.X = 0;
    coord.Y = row;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void printMessage(const std::string& message) {
    ConsoleColor::Print(message, ConsoleColor::CYAN);
}

// (Game state waiting moved to src/App/game_state_waiter.hpp)

int main() {
    // ===== INITIALIZE LOGGER FIRST =====
    if (!Logger::Initialize()) {
        ConsoleColor::PrintWarning("Failed to initialize logger - proceeding without file logging");
    } else {
        ConsoleColor::PrintSuccess("Logger initialized - session logged to Logs.txt");
    }
    
    ConsoleColor::ClearScreen();
    ConsoleColor::PrintHeader("SUBARO INITIALIZING");
    Logger::LogHeader("SUBARO INITIALIZING");

RESTART_FROM_BEGINNING:  // Jump here when CS2 closes and user wants to restart
    
    // Reset global flags
    ResetAppState();

    // ===== VERSION CONTROL SYSTEM =====
    VersionManager::VersionConfig versionConfig = VersionManager::CheckAndUpdate();
    
    if (!versionConfig.isValid) {
        ConsoleColor::PrintError("Failed to fetch version configuration");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    // Check if we need to download/update
    std::string localVersion = VersionManager::ReadLocalVersion();
    bool needsUpdate = VersionManager::IsVersionMismatch(localVersion, versionConfig.version);
    bool filesPresent = VersionManager::AreDriverFilesPresent();

    if (needsUpdate || !filesPresent) {
        ConsoleColor::PrintInfo("Downloading driver package...");
        
        if (!DownloadHelper::DownloadAndExtractDriverPackage(versionConfig.downloadUrl)) {
            ConsoleColor::PrintError("Failed to download driver package");
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }

        if (!VersionManager::VerifyInstallation(versionConfig)) {
            ConsoleColor::PrintError("Installation verification failed");
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }

        ConsoleColor::PrintSuccess("Driver package updated");
    } else {
        ConsoleColor::PrintSuccess("Version check passed");
    }

    // ===== AUTO-LOADER SYSTEM =====
    driver::DriverHandle driver_handle(versionConfig.deviceSymbolicLink);
    
    Logger::LogInfo("Attempting to connect to driver...");
    Logger::Log("[*] Device symbolic link: " + versionConfig.deviceSymbolicLink);
    
    if (!DriverManager::ConnectOrLoadDriver(driver_handle, versionConfig)) {
        ConsoleColor::PrintError("Failed to connect or load driver");
        Logger::LogError("Failed to connect or load driver");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    
    Logger::LogSuccess("Driver connection established");

    // ===== INITIALIZE OFFSET MANAGER =====
    ConsoleColor::PrintInfo("Initializing offsets...");
    if (!OffsetsManager::Initialize()) {
        ConsoleColor::PrintError("Failed to initialize offsets");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    ConsoleColor::PrintSuccess("Offsets initialized");

    // Clear console and show logo
    ConsoleColor::ClearScreen();
    ConsoleColor::PrintLogo();
    ConsoleColor::HideCursor();
    
    std::cout << "\n";
    ConsoleColor::PrintInfo("Waiting for CS2...");

    // ===== WAIT FOR CS2.EXE =====
    const std::wstring proc_name = L"cs2.exe";
    Logger::LogInfo("Waiting for CS2 process...");
    DWORD pid = ProcessUtils::WaitForProcess(proc_name);
    g_cs2ProcessId = pid;
    Logger::LogSuccess("CS2 process found - PID: " + std::to_string(pid));

    // Start CS2 process monitor thread
    std::thread processMonitorThread(ProcessUtils::CS2ProcessMonitor);
    processMonitorThread.detach();
    Logger::LogInfo("CS2 process monitor started");

    if (!driver_handle.attach_to_process(reinterpret_cast<HANDLE>(static_cast<uint64_t>(pid)))) {
        ConsoleColor::PrintError("Attach failed");
        Logger::LogError("Failed to attach to CS2 process");
        std::cin.get();
        return 1;
    }

    ConsoleColor::PrintSuccess("Attached to CS2");
    Logger::LogSuccess("Attached to CS2 process");

    // ===== WAIT FOR CLIENT.DLL =====
    std::uintptr_t client = 0;
    int clientAttempts = 0;
    ConsoleColor::PrintInfo("Waiting for client.dll...");
    Logger::LogInfo("Waiting for client.dll...");
    
    while (clientAttempts < 100) {
        client = driver_handle.get_module_base(pid, L"client.dll");
        if (client != 0) {
            ConsoleColor::PrintSuccess("client.dll found");
            Logger::LogSuccess("client.dll found at address: 0x" + std::to_string(client));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        clientAttempts++;
    }

    if (client == 0) {
        ConsoleColor::PrintError("client.dll not found");
        Logger::LogError("client.dll not found after 100 attempts");
        std::cin.get();
        return 1;
    }

    // ===== IMGUI ESP INITIALIZATION (ONE TIME) =====
    ConsoleColor::PrintInfo("Initializing ESP overlay...");
    
    ImGuiESP espRenderer(driver_handle, client);
    
    std::atomic<bool> espRunning(false);
    std::thread espThread([&]() {
        try {
            espRunning = true;
            
            OSImGui::OSImGui gui;
            gui.AttachAnotherWindow("Counter-Strike 2", "", [&]() {
                espRenderer.updateViewMatrix();
                espRenderer.render();
            });
        }
        catch (const std::exception& e) {
            ConsoleColor::PrintError(std::string("ESP thread failed: ") + e.what());
            espRunning = false;
        }
    });
    
    espThread.detach();  // Detach thread so it runs independently
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!espRunning) {
        ConsoleColor::PrintError("Failed to initialize ESP overlay");
    } else {
        ConsoleColor::PrintSuccess("ESP overlay initialized");
    }
    
    // ===== INITIALIZE KERNEL MOUSE (ONE TIME) =====
    ConsoleColor::PrintInfo("Initializing aimbot...");
    if (!KernelMouse::Initialize(L"\\\\.\\airway_radio")) {
        ConsoleColor::PrintWarning("Aimbot disabled - camera control unavailable");
    } else {
        ConsoleColor::PrintSuccess("Aimbot ready");
    }
    
    // ===== INITIALIZE FEATURES (ONE TIME) =====
    Triggerbot triggerbot(driver_handle, client);
    BunnyHop bhop(driver_handle, client);
    Aimbot aimbot(driver_handle, client);
    Chams chams(driver_handle, client);
    
    // NEW FEATURES INITIALIZATION
    BoneESP boneESP(driver_handle, client);
    ThirdPerson thirdPerson(driver_handle, client);
    RecoilCompensation recoilComp(driver_handle, client);
    SmoothAim smoothAim(5.0f); // 5.0f = default smoothness factor
    EntityPredictor entityPredictor(driver_handle);
    VisibilityChecker visibilityChecker(driver_handle, client);
    SilentAim silentAim(driver_handle, client);
    
    // ===== LOAD SETTINGS FROM FILE =====
    SettingsManager::Settings savedSettings = SettingsManager::LoadSettings();
    
    bool triggerbotEnabled = savedSettings.triggerbotEnabled;
    bool bhopEnabled = savedSettings.bhopEnabled;
    bool aimbotEnabled = savedSettings.aimbotEnabled;
    bool consoleDebugEnabled = savedSettings.consoleDebugEnabled;
    bool teamCheckEnabled = savedSettings.teamCheckEnabled;
    bool headAngleLineEnabled = savedSettings.headAngleLineEnabled;
    bool snaplinesEnabled = savedSettings.snaplinesEnabled;
    bool distanceESPEnabled = savedSettings.distanceESPEnabled;
    bool snaplinesWallCheckEnabled = savedSettings.snaplinesWallCheckEnabled;
    bool chamsEnabled = savedSettings.chamsEnabled;
    
    // NEW EXTENDED FEATURES
    bool boneESPEnabled = savedSettings.boneESPEnabled;
    bool silentAimEnabled = savedSettings.silentAimEnabled;
    bool thirdPersonEnabled = savedSettings.thirdPersonEnabled;
    bool recoilCompEnabled = savedSettings.recoilCompEnabled;
    bool smoothAimEnabled = savedSettings.smoothAimEnabled;
    bool entityPredictorEnabled = savedSettings.entityPredictorEnabled;
    bool visibilityCheckEnabled = savedSettings.visibilityCheckEnabled;
    
    if (SettingsManager::SettingsFileExists()) {
        ConsoleColor::PrintSuccess("Settings loaded from file");
        Logger::LogSuccess("Settings loaded from saburo_settings.cfg");
    } else {
        ConsoleColor::PrintInfo("No saved settings found - using defaults");
        Logger::LogInfo("Using default settings");
    }
    
RESTART_GAME_DETECTION:  // Re-attack label - jump here when player leaves game
    
    // Enable lobby mode
    espRenderer.setLobbyMode(true);
    
    Logger::LogInfo("Entering lobby mode - waiting for game start");
    
    if (!GameStateWaiter::WaitForInGameState(driver_handle, client, espRenderer)) {
        // CS2 process closed while waiting
        ConsoleColor::ClearScreen();
        ConsoleColor::PrintLogo();
        ConsoleColor::HideCursor();
        
        std::cout << "\n";
        ConsoleColor::PrintWarning("CS2 process has been closed!");
        Logger::LogWarning("CS2 process closed during lobby wait");
        std::cout << "\n";
        
        std::cout << "Would you like to close Saburo as well? (Y/N): ";
        
        char response;
        std::cin >> response;
        std::cin.ignore(); // Clear newline
        
        if (response == 'Y' || response == 'y') {
            ConsoleColor::PrintInfo("Shutting down Saburo...");
            Logger::LogInfo("User chose to exit after CS2 closure");
            g_shutdownRequested.store(true);
            
            KernelMouse::Shutdown();
            Logger::LogInfo("Shutting down Subaro...");
            Logger::Shutdown();
            
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 0;
        } else {
            ConsoleColor::PrintInfo("Restarting detection sequence...");
            Logger::LogInfo("User chose to restart after CS2 closure");
            g_shutdownRequested.store(true);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Jump to beginning
            ConsoleColor::ClearScreen();
            goto RESTART_FROM_BEGINNING;
        }
    }
    
    // Disable lobby mode when game starts
    espRenderer.setLobbyMode(false);
    Logger::LogSuccess("In-game mode activated");
    
    // Give game state a moment to fully stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Track in-game state for UI color switching
    bool isInGame = true;

    // ===== RE-ENABLE FEATURES AFTER RE-ATTACK =====
    // Use ToggleManager to apply all toggles consistently
    ToggleManager toggleManager(
        triggerbot,
        bhop,
        aimbot,
        chams,
        boneESP,
        thirdPerson,
        visibilityChecker,
        smoothAim,
        recoilComp,
        espRenderer,
        triggerbotEnabled,
        bhopEnabled,
        aimbotEnabled,
        consoleDebugEnabled,
        teamCheckEnabled,
        headAngleLineEnabled,
        snaplinesEnabled,
        distanceESPEnabled,
        snaplinesWallCheckEnabled,
        chamsEnabled,
        boneESPEnabled,
        silentAimEnabled,
        thirdPersonEnabled,
        recoilCompEnabled,
        smoothAimEnabled,
        entityPredictorEnabled,
        visibilityCheckEnabled
    );
    
    toggleManager.applyAll();
    // Reset states on respawn
    recoilComp.resetRecoil();
    if (!smoothAimEnabled) {
        smoothAim.reset();
    }
    
    // Hide cursor for cleaner display

    // ===== GET LOCAL PLAYER =====
    int attempts = 0;
    const int max_attempts = 50;
    std::uintptr_t local_player_pawn = 0;

    while (attempts < max_attempts) {
        std::uintptr_t local_controller = driver_handle.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);

        if (local_controller == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
            continue;
        }

        uint32_t local_player_handle = driver_handle.read<uint32_t>(local_controller + OffsetsManager::Get().m_hPlayerPawn);

        if (local_player_handle == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
            continue;
        }

        local_player_pawn = EntityUtils::get_entity_from_handle(driver_handle, client, local_player_handle);

        if (local_player_pawn == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
            continue;
        }

        // ===== START ENTITY ENUMERATION =====
        EntityEnumerator enumerator(driver_handle, client);
        
        // TRY MULTIPLE TIMES: Entity enumeration might fail on first attempt
        EntityManager entities;
        bool entityScanSuccess = false;
        
        for (int scanAttempt = 0; scanAttempt < 10; scanAttempt++) {
            entities = enumerator.enumerate_all_entities(local_player_pawn);
            
            // Success if we have ANY entities OR we're in a valid game state (could be alone)
            if (entities.all_entities.size() > 0) {
                entityScanSuccess = true;
                ConsoleColor::PrintSuccess("Entity scan successful - " + std::to_string(entities.alive_entities.size()) + " players detected");
                Logger::LogSuccess("Entity scan successful - " + std::to_string(entities.alive_entities.size()) + " players detected");
                break;
            }
            
            // If 0 entities, verify we're still in a valid game state
            std::uintptr_t checkController = driver_handle.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
            if (checkController != 0) {
                uint32_t checkHandle = driver_handle.read<uint32_t>(checkController + OffsetsManager::Get().m_hPlayerPawn);
                if (checkHandle != 0) {
                    std::uintptr_t checkPawn = EntityUtils::get_entity_from_handle(driver_handle, client, checkHandle);
                    if (checkPawn != 0) {
                        int32_t checkHealth = driver_handle.read<int32_t>(checkPawn + OffsetsManager::Get().m_iHealth);
                        
                        // If we have valid health, we're in-game (might be alone in server)
                        if (checkHealth > 0) {
                            if (scanAttempt == 9) {
                                // Last attempt - accept 0 entities if we're in a valid game state
                                entityScanSuccess = true;
                                ConsoleColor::PrintWarning("No other players detected - continuing (empty server or practice mode)");
                                Logger::LogWarning("No other players detected - continuing");
                                break;
                            } else {
                                // Wait and retry
                                ConsoleColor::PrintInfo("Entity scan returned 0 - retrying (" + std::to_string(scanAttempt + 1) + "/10)...");

                                // Wait a shorter time to make this more responsive
                                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                                continue;
                            }
                        }
                    }
                }
            }
            
            // Not in valid game state - wait and retry
            if (scanAttempt < 9) {
                ConsoleColor::PrintWarning("Game state not ready - retrying entity scan (" + std::to_string(scanAttempt + 1) + "/10)...");

                // Wait a shorter time to make this more responsive
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        if (!entityScanSuccess) {
            ConsoleColor::PrintError("Entity scan failed after 10 attempts - restarting detection");
            Logger::LogError("Entity scan failed after 10 attempts");
            attempts++;
            continue;
        }

        // Clear screen and show unified panel
        // Only clear and hide cursor once per game session, not every frame
        static bool clearedForGame = false;
        if (!clearedForGame) {
            ConsoleColor::ClearScreen();
            ConsoleColor::HideCursor();
            clearedForGame = true;
        }

        // ===== MAIN LOOP =====
        
        triggerbot.setLocalPlayerPawn(local_player_pawn);
        bhop.setLocalPlayerPawn(local_player_pawn);
        aimbot.setLocalPlayerPawn(local_player_pawn);

        int frame = 0;
        const int target_fps = 60;
        const int frame_time_ms = 1000 / target_fps;
        
        bool shouldRestart = false;

        while (true) {
            auto frame_start = std::chrono::high_resolution_clock::now();

            // Auto-reinitialize local player
            {
                std::uintptr_t current_controller = driver_handle.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
                
                if (current_controller != 0) {
                    uint32_t current_handle = driver_handle.read<uint32_t>(current_controller + OffsetsManager::Get().m_hPlayerPawn);
                    
                    if (current_handle != 0) {
                        std::uintptr_t new_pawn = EntityUtils::get_entity_from_handle(driver_handle, client, current_handle);
                        
                        if (new_pawn != 0 && new_pawn != local_player_pawn) {
                            local_player_pawn = new_pawn;
                            
                            triggerbot.setLocalPlayerPawn(local_player_pawn);
                            bhop.setLocalPlayerPawn(local_player_pawn);
                            aimbot.setLocalPlayerPawn(local_player_pawn);
                            
                            if (consoleDebugEnabled) {
                                ConsoleColor::PrintInfo("Local player re-initialized");
                            }
                        }
                    }
                }
            }

            // Check for key presses
            if (_kbhit()) {
                int key = _getch();
                toggleManager.handleKey(key);
            }

            // ===== CHECK CS2 PROCESS STATUS =====
            if (!g_cs2ProcessRunning.load()) {
                // CS2 has closed - prompt user
                ConsoleColor::ClearScreen();
                ConsoleColor::PrintLogo();
                
                std::cout << "\n";
                ConsoleColor::PrintWarning("CS2 process has been closed!");
                Logger::LogWarning("CS2 process closed - prompting user");
                std::cout << "\n";
                
                std::cout << "Would you like to close Saburo as well? (Y/N): ";
                
                char response;
                std::cin >> response;
                std::cin.ignore(); // Clear newline
                
                if (response == 'Y' || response == 'y') {
                    ConsoleColor::PrintInfo("Shutting down Saburo...");
                    Logger::LogInfo("User chose to exit after CS2 closure");
                    g_shutdownRequested.store(true);
                    break;
                } else {
                    ConsoleColor::PrintInfo("Restarting detection sequence...");
                    Logger::LogInfo("User chose to restart after CS2 closure");
                    g_shutdownRequested.store(true);
                    
                    // Clean up current session
                    aimbot.setEnabled(false);
                    triggerbot.setEnabled(false);
                    bhop.setEnabled(false);
                    chams.setEnabled(false);
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    // Jump to beginning
                    ConsoleColor::ClearScreen();
                    goto RESTART_FROM_BEGINNING;
                }
            }

            enumerator.update_entities(entities);
            espRenderer.updateEntities(entities, local_player_pawn);

            // Update chams
            if (chamsEnabled) {
                chams.update(entities, local_player_pawn);
            }
            
            // Update triggerbot enemy cache from ESP entities (BEFORE triggerbot.update)
            if (triggerbotEnabled) {
                triggerbot.updateEnemyCache(entities);
            }
            
            // ===== IMPROVED RE-ATTACH DETECTION =====
            static int noPlayersFrameCount = 0;
            static constexpr int QUICK_RESCAN_THRESHOLD = 30; // 0.5s at 60 FPS
            static constexpr int FORCE_RESCAN_THRESHOLD = 120; // 2s at 60 FPS
            static constexpr int FULL_RESTART_THRESHOLD = 300; // 5s at 60 FPS
            
            // Check if we have no players detected
            bool noPlayers = (entities.all_entities.size() == 0);
            
            if (noPlayers) {
                noPlayersFrameCount++;
                
                // QUICK RESCAN: Try updating entities again
                if (noPlayersFrameCount == QUICK_RESCAN_THRESHOLD) {
                    ConsoleColor::PrintWarning("No entities detected - quick rescan...");
                    Logger::LogWarning("No entities detected - quick rescan");
                    enumerator.update_entities(entities);
                    
                    if (entities.all_entities.size() > 0) {
                        noPlayersFrameCount = 0;
                        ConsoleColor::PrintSuccess("Quick rescan successful - " + std::to_string(entities.all_entities.size()) + " players found");
                        Logger::LogSuccess("Quick rescan successful");
                    }
                }
                
                // FORCE RESCAN: Clear cache and do full enumeration
                if (noPlayersFrameCount == FORCE_RESCAN_THRESHOLD) {
                    ConsoleColor::PrintWarning("Entity list empty for 2s - forcing full rescan...");
                    Logger::LogWarning("Entity list empty - forcing full rescan");
                    
                    // Verify local player is still valid
                    std::uintptr_t checkController = driver_handle.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
                    if (checkController != 0) {
                        uint32_t checkHandle = driver_handle.read<uint32_t>(checkController + OffsetsManager::Get().m_hPlayerPawn);
                        if (checkHandle != 0) {
                            std::uintptr_t checkPawn = EntityUtils::get_entity_from_handle(driver_handle, client, checkHandle);
                            if (checkPawn != 0) {
                                // Force full rescan with current local player
                                enumerator.force_rescan(entities, checkPawn);
                                
                                if (entities.all_entities.size() > 0) {
                                    noPlayersFrameCount = 0;
                                    ConsoleColor::PrintSuccess("Force rescan successful - " + std::to_string(entities.all_entities.size()) + " players found");
                                    Logger::LogSuccess("Force rescan successful - " + std::to_string(entities.all_entities.size()) + " players found");
                                }
                            }
                        }
                    }
                }
                
                // FULL RESTART: Return to lobby detection if still no players
                if (noPlayersFrameCount >= FULL_RESTART_THRESHOLD) {
                    ConsoleColor::ClearScreen();
                    
                    Logger::LogInfo("Re-attach triggered - no players for 5 seconds");
                    
                    UIHelper::PrintReattachNotice();
                    
                    aimbot.setEnabled(false);
                    triggerbot.setEnabled(false);
                    bhop.setEnabled(false);
                    chams.setEnabled(false);
                    
                    local_player_pawn = 0;
                    noPlayersFrameCount = 0;
                    shouldRestart = true;
                    
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    
                    ConsoleColor::PrintInfo("Returning to game state detection...\n");
                    Logger::LogInfo("Returning to game state detection...");
                    
                    break;
                }
            } else {
                // Reset counter if we have players
                noPlayersFrameCount = 0;
            }
            
            if (triggerbotEnabled) {
                triggerbot.update();
            }
            
            if (bhopEnabled) {
                bhop.update();
            }
            
            if (aimbotEnabled) {
                aimbot.update(entities);
            }

            // Status line - now vertical display every 60 frames
            if (frame % 60 == 0) {
                UIHelper::PrintUnifiedPanel(
                    frame,
                    static_cast<int>(entities.all_entities.size()),
                    static_cast<int>(entities.enemy_entities.size()),
                    static_cast<int>(entities.teammate_entities.size()),
                    aimbotEnabled,
                    triggerbotEnabled,
                    bhopEnabled,
                    headAngleLineEnabled,
                    snaplinesEnabled,
                    distanceESPEnabled,
                    snaplinesWallCheckEnabled,
                    teamCheckEnabled,
                    chamsEnabled,
                    boneESPEnabled,
                    silentAimEnabled,
                    thirdPersonEnabled,
                    recoilCompEnabled,
                    smoothAimEnabled,
                    entityPredictorEnabled,
                    visibilityCheckEnabled,
                    isInGame
                );
            }

            frame++;

            auto frame_end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
            int sleep_time = frame_time_ms - static_cast<int>(elapsed);

            if (sleep_time > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            }
        }
        
        // After breaking out of main loop, check if we should restart
        if (shouldRestart) {
            // Jump back to game detection
            goto RESTART_GAME_DETECTION;
        }

        break;
    }

    KernelMouse::Shutdown();
    
    Logger::LogInfo("Shutting down Subaro...");
    Logger::Shutdown();

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}
