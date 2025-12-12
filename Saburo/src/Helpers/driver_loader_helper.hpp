#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <chrono>
#include "download_helper.hpp"
#include "console_colors.hpp"
#include "driver_patcher.hpp" // deprecated here (no patching is performed)
#include "logger.hpp"

namespace DriverLoaderHelper {

    // Fixed provider selection: lock to 52 (stable for this system)
    inline int GetRandomProviderID() {
        constexpr int provider = 52;
        ConsoleColor::PrintInfo("Selected KDU provider: 52");
        Logger::LogInfo("Selected KDU provider: 52");
        return provider;
    }

    inline bool WaitForDeviceReady(const std::string& deviceName, int timeoutMs = 5000) {
        std::string path = R"(\\.\)" + deviceName;
        const int stepMs = 100;
        for (int t = 0; t <= timeoutMs; t += stepMs) {
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                ConsoleColor::PrintSuccess("Device ready: " + path);
                Logger::LogSuccess("Device ready: " + path);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        }
        ConsoleColor::PrintWarning("Device not ready within timeout: " + path);
        Logger::LogWarning("Device not ready within timeout: " + path);
        return false;
    }

    // Load driver using KDU (Kernel Driver Utility) - executes as external admin command
    // deviceName: The custom device name to patch into the driver before loading
    inline bool LoadDriverWithKDU(const std::string& deviceName = "airway_radio") {
        std::string exeDir = DownloadHelper::GetExecutableDirectory();
        std::string kduPath = exeDir + "\\kdu.exe";
        std::string sysPath = exeDir + "\\Hiroyoshi.sys";

        Logger::LogInfo("Loading driver with KDU (no on-disk patching)...");
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

                // NO PATCHING: Do not modify the .sys on disk — using original .sys
                ConsoleColor::PrintInfo("Skipping driver patching step (using original .sys)");
        Logger::LogInfo("Skipping driver patching step (using original .sys)");

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
                // Wait until the device is actually ready
                if (WaitForDeviceReady(deviceName, 5000)) {
                    ConsoleColor::PrintSuccess("Driver loaded successfully with KDU");
                    Logger::LogSuccess("Driver loaded successfully with KDU (exit code: " + std::to_string(exitCode) + ")");
                    return true;
                } else {
                    ConsoleColor::PrintWarning("Device not ready after KDU success; retrying...");
                    Logger::LogWarning("Device not ready after KDU success; retrying...");
                    if (attempt < maxAttempts) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        continue;
                    }
                    return false;
                }
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
    inline bool UnloadDriverWithKDU(int providerID = 50) {
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
