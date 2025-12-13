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
#include "src/Helpers/console_logger.hpp"
#include "src/Helpers/kernel_mouse.hpp"
#include "src/Helpers/download_helper.hpp"
#include "src/Helpers/windows_security_helper.hpp"
#include "src/Helpers/driver_loader_helper.hpp"
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/version_manager.hpp"
#include "src/Helpers/logger.hpp"
#include "src/Helpers/settings_manager.hpp"
#include "src/Helpers/toggle_manager.hpp"
#include "src/Features/BoneESP.hpp"
#include "src/Features/ThirdPerson.hpp"
#include "src/Features/VisibilityChecker.hpp"
#include "src/Features/SmoothAim.hpp"
#include "src/Features/RecoilCompensation.hpp"
#include "src/Features/SilentAim.hpp"
#include "src/Features/EntityPredictor.hpp"
#include "app_entry.hpp"

namespace AppEntry {

// Global flag to signal CS2 process closure
static std::atomic<bool> g_cs2ProcessRunning(true);
static std::atomic<bool> g_shutdownRequested(false);
static DWORD g_cs2ProcessId = 0;

// Clear console without deleting all text (just scroll to new line)
static void clear_line() {
    std::cout << "\r                                                                                                                                                                                                                 ";
}

// Set console cursor position
static void set_cursor(int row) {
    COORD coord;
    coord.X = 0;
    coord.Y = row;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

static DWORD wait_for_process(const std::wstring& name) {
    std::cout << "[*] Waiting for " << std::string(name.begin(), name.end()) << "...";
    std::cout.flush();
    
    while (true) {
        DWORD pid = process::get_process_id_by_name(name);
        if (pid != 0) {
            std::cout << "\r[+] Found " << std::string(name.begin(), name.end()) << " (PID: " << pid << ")          \n";
            return pid;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// CS2 Process Monitor Thread
static void CS2ProcessMonitor() {
    while (!g_shutdownRequested.load()) {
        if (g_cs2ProcessId != 0) {
            DWORD pid = process::get_process_id_by_name(L"cs2.exe");
            if (pid == 0) {
                // CS2 process no longer exists
                g_cs2ProcessRunning.store(false);
                ConsoleColor::PrintWarning("CS2 process terminated!");
                Logger::LogWarning("CS2 process terminated - PID " + std::to_string(g_cs2ProcessId) + " no longer exists");
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Check every 1 second
    }
}

static std::uintptr_t get_entity_from_handle(driver::DriverHandle& drv, std::uintptr_t client, uint32_t handle) {
    if (handle == 0) {
        return 0;
    }

    const uint32_t index = handle & 0x7FFF;
    std::uintptr_t entity_list = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwEntityList);

    if (entity_list == 0) {
        return 0;
    }

    const uint32_t chunk_index = index >> 9;
    std::uintptr_t chunk_pointer_address = entity_list + (0x8 * chunk_index) + 0x10;
    std::uintptr_t list_entry = drv.read<std::uintptr_t>(chunk_pointer_address);

    if (list_entry == 0) {
        return 0;
    }

    const uint32_t entity_index = index & 0x1FF;
    std::uintptr_t entity_address = list_entry + (0x70 * entity_index);
    std::uintptr_t entity = drv.read<std::uintptr_t>(entity_address);

    return entity;
}

static void printMessage(const std::string& message) {
    ConsoleColor::Print(message, ConsoleColor::CYAN);
}

// Auto-loader: Check driver files and download if needed (version-aware)
static bool EnsureDriverFilesPresent(const VersionManager::VersionConfig& config) {
    if (VersionManager::AreDriverFilesPresent()) {
        ConsoleColor::PrintSuccess("Driver files already present");
        return true;
    }

    ConsoleColor::PrintInfo("Driver files not found - starting download...");
    
    if (!DownloadHelper::DownloadAndExtractDriverPackage(config.downloadUrl)) {
        ConsoleColor::PrintError("Failed to download or extract driver package");
        return false;
    }

    // Verify installation after download
    if (!VersionManager::VerifyInstallation(config)) {
        ConsoleColor::PrintError("Installation verification failed");
        return false;
    }

    return true;
}

// Auto-loader: Attempt to connect to driver or load it if needed (version-aware)
static bool ConnectOrLoadDriver(driver::DriverHandle& driver_handle, const VersionManager::VersionConfig& config) {
    // Try connecting to existing driver first
    if (driver_handle.is_valid()) {
        ConsoleColor::PrintSuccess("Driver already loaded and connected");
        Logger::LogSuccess("Driver already loaded and connected");
        return true;
    }

    ConsoleColor::PrintWarning("Driver not detected - loading...");
    Logger::LogWarning("Driver not detected - loading...");

    // Check Windows Security real-time protection
    if (!WindowsSecurityHelper::GuideUserToDisableRealtimeProtection()) {
        ConsoleColor::PrintError("Cannot proceed without disabling Real-Time Protection");
        Logger::LogError("Cannot proceed without disabling Real-Time Protection");
        return false;
    }

    // Ensure driver files are present
    if (!EnsureDriverFilesPresent(config)) {
        return false;
    }

    // Load driver with KDU (always use hardcoded device name)
    if (!DriverLoaderHelper::LoadDriverWithKDU("airway_radio")) {
        ConsoleColor::PrintError("Failed to load driver with KDU");
        Logger::LogError("Failed to load driver with KDU");
        return false;
    }

    // Try connecting multiple times after loading (driver needs time to initialize)
    const int maxConnectionAttempts = 5;
    for (int attempt = 1; attempt <= maxConnectionAttempts; attempt++) {
        ConsoleColor::PrintInfo("Connecting to driver (attempt " + std::to_string(attempt) + "/" + std::to_string(maxConnectionAttempts) + ")...");
        Logger::LogInfo("Connecting to driver (attempt " + std::to_string(attempt) + "/" + std::to_string(maxConnectionAttempts) + ")...");
        
        // Reconstruct driver handle to attempt new connection (hardcoded device)
        driver_handle.~DriverHandle();
        new (&driver_handle) driver::DriverHandle();

        if (driver_handle.is_valid()) {
            ConsoleColor::PrintSuccess("Successfully connected to driver");
            Logger::LogSuccess("Successfully connected to driver on attempt " + std::to_string(attempt));
            return true;
        }

        // Wait before retrying (increasing delay each attempt)
        if (attempt < maxConnectionAttempts) {
            int delayMs = attempt * 500; // 500ms, 1000ms, 1500ms, 2000ms, 2500ms
            ConsoleColor::PrintInfo("Waiting " + std::to_string(delayMs) + "ms before retry...");
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }

    ConsoleColor::PrintError("Failed to connect to driver after " + std::to_string(maxConnectionAttempts) + " attempts");
    Logger::LogError("Failed to connect to driver after " + std::to_string(maxConnectionAttempts) + " attempts");
    return false;
}

// Wait for player to be actually in-game (not menu/loading)
static bool WaitForInGameState(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& espRenderer) {
    using namespace ConsoleColor;
    
    const auto& offsets = OffsetsManager::Get();
    
    // FIRST: Check if we're already in-game (launched while match is running)
    std::uintptr_t quickCheckController = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
    if (quickCheckController != 0) {
        uint32_t quickCheckHandle = drv.read<uint32_t>(quickCheckController + offsets.m_hPlayerPawn);
        if (quickCheckHandle != 0) {
            std::uintptr_t quickCheckPawn = get_entity_from_handle(drv, client, quickCheckHandle);
            if (quickCheckPawn != 0) {
                int32_t quickCheckHealth = drv.read<int32_t>(quickCheckPawn + offsets.m_iHealth);
                uint8_t quickCheckTeam = drv.read<uint8_t>(quickCheckPawn + offsets.m_iTeamNum);
                
                // If we have valid health and team, we're already in-game
                if (quickCheckHealth > 0 && quickCheckHealth <= 150 && (quickCheckTeam == 2 || quickCheckTeam == 3)) {
                    ConsoleColor::PrintSuccess("Already in-game - ready to scan entities");
                    Logger::LogSuccess("Already in-game - ready to scan entities");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    return true;
                }
            }
        }
    }
    
    // NOT in-game yet - show lobby waiting screen
    ConsoleColor::ClearScreen();
    ConsoleColor::PrintLogo(false);  // false = lobby mode (blue)
    
    std::cout << "\n";
    ConsoleColor::PrintInfo("Waiting for in-game state...");
    Logger::LogInfo("Waiting for in-game state...");
    std::cout << "\n";
    SetColor(ConsoleColor::YELLOW);
    std::cout << "  Please JOIN A GAME:\n";
    Reset();
    std::cout << "    - Casual/Competitive Match\n";
    std::cout << "    - Deathmatch\n";
    std::cout << "    - Practice/Workshop Map\n";
    std::cout << "\n";
    SetColor(ConsoleColor::DARK_GRAY);
    std::cout << "  (Press ESC to cancel if CS2 closed)\n";
    Reset();
    std::cout << "\n";
    
    int consecutiveValidChecks = 0;
    const int requiredValidChecks = 3;
    
    while (true) {
        // ===== CHECK IF CS2 PROCESS CLOSED =====
        if (!g_cs2ProcessRunning.load()) {
            ConsoleColor::PrintWarning("CS2 process closed while waiting for game!");
            Logger::LogWarning("CS2 process closed during lobby wait");
            return false;
        }
        
        // Update lobby ESP each iteration
        std::uintptr_t lobbyLocalController = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
        if (lobbyLocalController != 0) {
            uint32_t lobbyHandle = drv.read<uint32_t>(lobbyLocalController + offsets.m_hPlayerPawn);
            if (lobbyHandle != 0) {
                std::uintptr_t lobbyLocalPawn = get_entity_from_handle(drv, client, lobbyHandle);
                if (lobbyLocalPawn != 0) {
                    // Set lobby local player for ESP
                    EntityManager lobbyEntities;  // Empty entity list
                    espRenderer.updateEntities(lobbyEntities, lobbyLocalPawn);
                }
            }
        }
        
        bool allChecksPass = true;
        
        // Check 1: Entity List exists
        std::uintptr_t entityList = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwEntityList);
        if (entityList == 0) {
            allChecksPass = false;
        }
        
        // Check 2: ViewMatrix is valid
        if (allChecksPass) {
            std::uintptr_t viewMatrixAddr = client + OffsetsManager::Get().dwViewMatrix;
            float viewMatrix[16] = {0};
            drv.read_memory(reinterpret_cast<PVOID>(viewMatrixAddr), viewMatrix, sizeof(viewMatrix));
            
            bool hasNonZero = false;
            for (int i = 0; i < 16; i++) {
                if (viewMatrix[i] != 0.0f && viewMatrix[i] != 1.0f) {
                    hasNonZero = true;
                    break;
                }
            }
            
            if (!hasNonZero) {
                allChecksPass = false;
            }
        }
        
        // Check 3: Local Player exists and is valid
        if (allChecksPass) {
            std::uintptr_t localController = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
            if (localController != 0) {
                uint32_t pawnHandle = drv.read<uint32_t>(localController + offsets.m_hPlayerPawn);
                if (pawnHandle != 0) {
                    std::uintptr_t localPlayerPawn = get_entity_from_handle(drv, client, pawnHandle);
                    
                    if (localPlayerPawn != 0) {
                        int32_t health = drv.read<int32_t>(localPlayerPawn + offsets.m_iHealth);
                        uint8_t team = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);
                        
                        // Valid if health is reasonable and team is set
                        if (health <= 0 || health > 150 || (team != 2 && team != 3)) {
                            allChecksPass = false;
                        }
                    } else {
                        allChecksPass = false;
                    }
                } else {
                    allChecksPass = false;
                }
            } else {
                allChecksPass = false;
            }
        }
        
        if (allChecksPass) {
            consecutiveValidChecks++;
            
            if (consecutiveValidChecks >= requiredValidChecks) {
                ConsoleColor::PrintSuccess("In-game state detected!");
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

int Run() {
    // ===== INITIALIZE LOGGER FIRST =====
    if (!Logger::Initialize()) {
        ConsoleColor::PrintWarning("Failed to initialize logger - proceeding without file logging");
    } else {
        ConsoleColor::PrintSuccess("Logger initialized - session logged to Logs.txt");
    }
    
    ConsoleColor::PrintHeader("SUBARO INITIALIZING");
    Logger::LogHeader("SUBARO INITIALIZING");

RESTART_FROM_BEGINNING:  // Jump here when CS2 closes and user wants to restart
    
    // Reset global flags
    g_cs2ProcessRunning.store(true);
    g_shutdownRequested.store(false);
    g_cs2ProcessId = 0;

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
        // Check Windows Security BEFORE any download to avoid AV deleting files
        ConsoleColor::PrintInfo("Checking Windows Security (Real-Time Protection)...");
        if (!WindowsSecurityHelper::GuideUserToDisableRealtimeProtection()) {
            ConsoleColor::PrintError("Cannot proceed while Real-Time Protection is enabled");
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }

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
    // Always use built-in device name (airway_radio) regardless of version config
    driver::DriverHandle driver_handle;
    
    Logger::LogInfo("Attempting to connect to driver...");
    Logger::Log("[*] Device symbolic link: airway_radio (internal)");
    
    if (!ConnectOrLoadDriver(driver_handle, versionConfig)) {
        ConsoleColor::PrintError("Failed to connect or load driver");
        Logger::LogError("Failed to connect or load driver");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    
    Logger::LogSuccess("Driver connection established");

    // ===== INITIALIZE OFFSET MANAGER =====
    ConsoleColor::PrintInfo("Initializing offsets...");
    bool offsetsOk = OffsetsManager::InitializeFromHeaders();
    if (!offsetsOk) offsetsOk = OffsetsManager::Initialize();
    if (!offsetsOk) {
        ConsoleColor::PrintError("Failed to initialize offsets");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    ConsoleColor::PrintSuccess("Offsets initialized");

    // Clear console and show logo
    ConsoleColor::ClearScreen();
    ConsoleColor::PrintLogo();
    
    std::cout << "\n";
    ConsoleColor::PrintInfo("Waiting for CS2...");

    // ===== WAIT FOR CS2.EXE =====
    const std::wstring proc_name = L"cs2.exe";
    Logger::LogInfo("Waiting for CS2 process...");
    DWORD pid = wait_for_process(proc_name);
    g_cs2ProcessId = pid;
    Logger::LogSuccess("CS2 process found - PID: " + std::to_string(pid));

    // Start CS2 process monitor thread
    std::thread processMonitorThread(CS2ProcessMonitor);
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
    bhop.setTargetProcessId(pid);
    bhop.setJumpDelayMs(10);
    Aimbot aimbot(driver_handle, client);
    Chams chams(driver_handle, client);
    BoneESP boneEsp(driver_handle, client);
    ThirdPerson thirdPerson(driver_handle, client);
    VisibilityChecker visibilityChecker(driver_handle, client);
    SmoothAim smoothAim; // default smoothness
    RecoilCompensation recoilComp(driver_handle, client);
    SilentAim silentAim(driver_handle, client);
    EntityPredictor entityPredictor(driver_handle);
    
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
    bool boneESPEnabled = false;
    bool silentAimEnabled = false;
    bool thirdPersonEnabled = false;
    bool recoilCompEnabled = false;
    bool smoothAimEnabled = false;
    bool entityPredictorEnabled = false;
    bool visibilityCheckEnabled = false;
    
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
    
    if (!WaitForInGameState(driver_handle, client, espRenderer)) {
        // CS2 process closed while waiting
        ConsoleColor::ClearScreen();
        ConsoleColor::PrintLogo();
        
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
        boneEsp,
        thirdPerson,
        visibilityChecker,
        smoothAim,
        recoilComp,
        espRenderer,
        silentAim,
        entityPredictor,
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
    
    triggerbot.setEnabled(triggerbotEnabled);
    bhop.setEnabled(bhopEnabled);
    aimbot.setEnabled(aimbotEnabled);
    chams.setEnabled(chamsEnabled);
    ConsoleLogger::setEnabled(consoleDebugEnabled);
    triggerbot.setTeamCheckEnabled(teamCheckEnabled);
    aimbot.setTeamCheckEnabled(teamCheckEnabled);
    espRenderer.setHeadAngleLineEnabled(headAngleLineEnabled);
    espRenderer.setHeadAngleLineTeamCheckEnabled(teamCheckEnabled);
    espRenderer.setSnaplinesEnabled(snaplinesEnabled);
    espRenderer.setSnaplinesWallCheckEnabled(snaplinesWallCheckEnabled);
    espRenderer.setDistanceESPEnabled(distanceESPEnabled);
    espRenderer.setChamsEnabled(chamsEnabled);
    
    // Hide cursor for cleaner display
    ConsoleColor::HideCursor();

    // ===== GET LOCAL PLAYER =====
    int attempts = 0;
    const int max_attempts = 50;
    std::uintptr_t local_player_pawn = 0;

    while (attempts < max_attempts) {
        std::uintptr_t local_controller = driver_handle.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);

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

        local_player_pawn = get_entity_from_handle(driver_handle, client, local_player_handle);

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
            std::uintptr_t checkController = driver_handle.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
            if (checkController != 0) {
                uint32_t checkHandle = driver_handle.read<uint32_t>(checkController + OffsetsManager::Get().m_hPlayerPawn);
                if (checkHandle != 0) {
                    std::uintptr_t checkPawn = get_entity_from_handle(driver_handle, client, checkHandle);
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
        ConsoleColor::ClearScreen();
        ConsoleColor::HideCursor();

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
                std::uintptr_t current_controller = driver_handle.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
                
                if (current_controller != 0) {
                    uint32_t current_handle = driver_handle.read<uint32_t>(current_controller + OffsetsManager::Get().m_hPlayerPawn);
                    
                    if (current_handle != 0) {
                        std::uintptr_t new_pawn = get_entity_from_handle(driver_handle, client, current_handle);
                        
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
                
                if (key == '0') {
                    consoleDebugEnabled = !consoleDebugEnabled;
                    ConsoleLogger::setEnabled(consoleDebugEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '1') {
                    aimbotEnabled = !aimbotEnabled;
                    aimbot.setEnabled(aimbotEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '2') {
                    triggerbotEnabled = !triggerbotEnabled;
                    triggerbot.setEnabled(triggerbotEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '3') {
                    bhopEnabled = !bhopEnabled;
                    bhop.setEnabled(bhopEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '4') {
                    headAngleLineEnabled = !headAngleLineEnabled;
                    espRenderer.setHeadAngleLineEnabled(headAngleLineEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '5') {
                    snaplinesEnabled = !snaplinesEnabled;
                    espRenderer.setSnaplinesEnabled(snaplinesEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '6') {
                    distanceESPEnabled = !distanceESPEnabled;
                    espRenderer.setDistanceESPEnabled(distanceESPEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '7') {
                    snaplinesWallCheckEnabled = !snaplinesWallCheckEnabled;
                    espRenderer.setSnaplinesWallCheckEnabled(snaplinesWallCheckEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '8') {
                    teamCheckEnabled = !teamCheckEnabled;
                    triggerbot.setTeamCheckEnabled(teamCheckEnabled);
                    aimbot.setTeamCheckEnabled(teamCheckEnabled);
                    espRenderer.setHeadAngleLineTeamCheckEnabled(teamCheckEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
                else if (key == '9') {
                    chamsEnabled = !chamsEnabled;
                    chams.setEnabled(chamsEnabled);
                    espRenderer.setChamsEnabled(chamsEnabled);
                    
                    // Save settings
                    SettingsManager::Settings settings;
                    settings.triggerbotEnabled = triggerbotEnabled;
                    settings.bhopEnabled = bhopEnabled;
                    settings.aimbotEnabled = aimbotEnabled;
                    settings.consoleDebugEnabled = consoleDebugEnabled;
                    settings.teamCheckEnabled = teamCheckEnabled;
                    settings.headAngleLineEnabled = headAngleLineEnabled;
                    settings.snaplinesEnabled = snaplinesEnabled;
                    settings.distanceESPEnabled = distanceESPEnabled;
                    settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
                    settings.chamsEnabled = chamsEnabled;
                    SettingsManager::SaveSettings(settings);
                }
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
                    std::uintptr_t checkController = driver_handle.read<std::uintptr_t>(client + OffsetsManager::Get().dwLocalPlayerController);
                    if (checkController != 0) {
                        uint32_t checkHandle = driver_handle.read<uint32_t>(checkController + OffsetsManager::Get().m_hPlayerPawn);
                        if (checkHandle != 0) {
                            std::uintptr_t checkPawn = get_entity_from_handle(driver_handle, client, checkHandle);
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
                    
                    std::vector<std::string> boxLines = {
                        "",
                        "No players detected for 5 seconds.",
                        "Returning to lobby detection...",
                        ""
                    };
                    
                    ConsoleColor::PrintBox("RE-ATTACH TRIGGERED", boxLines);
                    
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


            // Third Person reapply per frame when enabled
            if (thirdPersonEnabled) {
                thirdPerson.update(local_player_pawn);
            }

            // Silent Aim: only active in third-person; reapply per frame if enabled
            if (silentAimEnabled && thirdPersonEnabled) {
                silentAim.update();
            } else if (silentAimEnabled && !thirdPersonEnabled) {
                // Ensure silent aim is disabled when third-person is off
                silentAim.setEnabled(false);
                silentAimEnabled = false;
            }

            // Status line - now vertical display every 60 frames
            if (frame % 60 == 0) {
                ConsoleColor::PrintUnifiedPanel(
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
                    consoleDebugEnabled,
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

} // namespace AppEntry
