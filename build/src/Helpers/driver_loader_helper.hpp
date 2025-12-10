#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <chrono>
#include "download_helper.hpp"
#include "console_colors.hpp"
#include "driver_patcher.hpp"
#include "logger.hpp"

namespace DriverLoaderHelper {

    // Get random provider ID from the three supported options - CALLED EACH TIME
    inline int GetRandomProviderID() {
        static const int providers[] = { 50, 44, 52 };  // Replaced 30 (AMD Ryzen - gets stuck) with 50
        
        // Use current time as seed for true randomness each call
        auto now = std::chrono::high_resolution_clock::now();
        auto seed = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        std::mt19937 gen(static_cast<unsigned int>(seed));
        std::uniform_int_distribution<> dis(0, 2);
        
        int selectedProvider = providers[dis(gen)];
        ConsoleColor::PrintInfo("Selected KDU provider: " + std::to_string(selectedProvider));
        Logger::LogInfo("Selected KDU provider: " + std::to_string(selectedProvider));
        
        return selectedProvider;
    }

    // Load driver using KDU (Kernel Driver Utility) - executes as external admin command
    // deviceName: The custom device name to patch into the driver before loading
    inline bool LoadDriverWithKDU(const std::string& deviceName = "airway_radio") {
        std::string exeDir = DownloadHelper::GetExecutableDirectory();
        std::string kduPath = exeDir + "\\kdu.exe";
        std::string sysPath = exeDir + "\\Hiroyoshi.sys";

        Logger::LogInfo("Loading driver with KDU...");
        Logger::Log("[*] Device name: " + deviceName);
        Logger::Log("[*] KDU path: " + kduPath);
        Logger::Log("[*] Driver path: " + sysPath);

        // Verify required files exist
        if (!DownloadHelper::FileExists(kduPath)) {
            ConsoleColor::PrintError("kdu.exe not found at: " + kduPath);
            Logger::LogError("kdu.exe not found at: " + kduPath);
            return false;
        }

        if (!DownloadHelper::FileExists(sysPath)) {
            ConsoleColor::PrintError("Hiroyoshi.sys not found at: " + sysPath);
            Logger::LogError("Hiroyoshi.sys not found at: " + sysPath);
            return false;
        }

        // PATCH THE DRIVER BEFORE LOADING (only if needed)
        if (deviceName != "airway_radio") {
            // Check if already patched
            if (DriverPatcher::IsDriverAlreadyPatched(sysPath, deviceName)) {
                ConsoleColor::PrintSuccess("Driver already patched with device name: " + deviceName);
                Logger::LogSuccess("Driver already patched with device name: " + deviceName);
            } else {
                // CREATE BACKUP BEFORE PATCHING
                if (!DriverPatcher::CreateDriverBackup(sysPath)) {
                    ConsoleColor::PrintWarning("Failed to create backup");
                    Logger::LogWarning("Failed to create backup");
                }

                // RESTORE FROM BACKUP FIRST (ensure we start with clean driver)
                std::string backupPath = sysPath + ".original";
                if (DownloadHelper::FileExists(backupPath)) {
                    DriverPatcher::RestoreOriginalDriver(sysPath);
                }

                // PATCH THE DRIVER
                if (!DriverPatcher::PatchDriverDeviceName(sysPath, deviceName)) {
                    ConsoleColor::PrintError("Failed to patch driver");
                    Logger::LogError("Failed to patch driver");
                    DriverPatcher::RestoreOriginalDriver(sysPath);
                    return false;
                }
                
                ConsoleColor::PrintSuccess("Driver patched: " + deviceName);
                Logger::LogSuccess("Driver patched: " + deviceName);
            }
        }

        // Try up to 3 times if KDU gets stuck
        const int maxAttempts = 3;
        for (int attempt = 1; attempt <= maxAttempts; attempt++) {
            if (attempt > 1) {
                ConsoleColor::PrintWarning("KDU attempt " + std::to_string(attempt) + " of " + std::to_string(maxAttempts));
                Logger::LogWarning("KDU attempt " + std::to_string(attempt) + " of " + std::to_string(maxAttempts));
            }

            // Get random provider ID (changes each attempt)
            int providerID = GetRandomProviderID();

            // Build KDU command: kdu.exe -prv <ID> -map "Hiroyoshi.sys"
            std::string command = "\"" + kduPath + "\" -prv " + std::to_string(providerID) + " -map \"" + sysPath + "\"";
            
            Logger::LogInfo("Executing KDU command...");
            Logger::Log("[*] Command: " + command);

            // Execute KDU
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));

            // Log: Starting KDU execution
            Logger::LogInfo("Starting KDU process...");

            if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                ConsoleColor::PrintError("Failed to execute KDU");
                Logger::LogError("Failed to execute KDU - CreateProcessA failed");
                
                if (attempt < maxAttempts) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
                return false;
            }

            // Wait for KDU to complete with 30 second timeout
            DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);

            if (waitResult == WAIT_TIMEOUT) {
                ConsoleColor::PrintWarning("KDU timed out - terminating and retrying...");
                Logger::LogWarning("KDU timed out after 30 seconds");
                
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                if (attempt < maxAttempts) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
                
                ConsoleColor::PrintError("KDU failed after " + std::to_string(maxAttempts) + " attempts");
                Logger::LogError("KDU failed after " + std::to_string(maxAttempts) + " attempts");
                return false;
            }

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            Logger::LogInfo("KDU process completed with exit code: " + std::to_string(exitCode));

            // KDU EXIT CODES:
            // 0 = Success (driver loaded)
            // 1 = Success with message "Bye-bye!" (also means driver loaded successfully)
            // Other = Actual failure
            if (exitCode == 0 || exitCode == 1) {
                ConsoleColor::PrintSuccess("Driver loaded successfully with KDU");
                Logger::LogSuccess("Driver loaded successfully with KDU (exit code: " + std::to_string(exitCode) + ")");
                return true;
            } else {
                ConsoleColor::PrintWarning("KDU returned exit code: " + std::to_string(exitCode));
                Logger::LogWarning("KDU returned exit code: " + std::to_string(exitCode));
                
                if (attempt < maxAttempts) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
                
                ConsoleColor::PrintError("KDU failed after " + std::to_string(maxAttempts) + " attempts");
                Logger::LogError("KDU failed with exit code: " + std::to_string(exitCode) + " after " + std::to_string(maxAttempts) + " attempts");
                return false;
            }
        }

        return false;
    }

    // Unload driver using KDU
    inline bool UnloadDriverWithKDU(int providerID = 44) {
        std::string exeDir = DownloadHelper::GetExecutableDirectory();
        std::string kduPath = exeDir + "\\kdu.exe";

        Logger::LogInfo("Unloading driver with KDU...");
        
        if (!DownloadHelper::FileExists(kduPath)) {
            ConsoleColor::PrintError("kdu.exe not found");
            Logger::LogError("kdu.exe not found");
            return false;
        }

        // Build KDU command: kdu.exe -prv <ID> -u
        std::string command = "\"" + kduPath + "\" -prv " + std::to_string(providerID) + " -u";
        
        Logger::Log("[*] Unload command: " + command);

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            ConsoleColor::PrintError("Failed to execute KDU for unload");
            Logger::LogError("Failed to execute KDU for unload");
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        Logger::LogInfo("KDU unload completed with exit code: " + std::to_string(exitCode));

        // Accept both 0 and 1 as success
        if (exitCode == 0 || exitCode == 1) {
            ConsoleColor::PrintSuccess("Driver unloaded successfully");
            Logger::LogSuccess("Driver unloaded successfully");
            return true;
        } else {
            ConsoleColor::PrintError("KDU unload failed with exit code: " + std::to_string(exitCode));
            Logger::LogError("KDU unload failed with exit code: " + std::to_string(exitCode));
            return false;
        }
    }
}
