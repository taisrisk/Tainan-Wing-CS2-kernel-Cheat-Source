#pragma once

#include <string>
#include <thread>
#include <chrono>
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/version_manager.hpp"
#include "src/Helpers/download_helper.hpp"
#include "src/Helpers/windows_security_helper.hpp"
#include "src/Helpers/driver_loader_helper.hpp"
#include "src/Helpers/logger.hpp"
#include "src/Core/driver.hpp"

namespace DriverManager {
    inline bool EnsureDriverFilesPresent(const VersionManager::VersionConfig& config) {
        if (VersionManager::AreDriverFilesPresent()) {
            ConsoleColor::PrintSuccess("Driver files already present");
            return true;
        }
        ConsoleColor::PrintInfo("Driver files not found - starting download...");
        if (!DownloadHelper::DownloadAndExtractDriverPackage(config.downloadUrl)) {
            ConsoleColor::PrintError("Failed to download or extract driver package");
            return false;
        }
        if (!VersionManager::VerifyInstallation(config)) {
            ConsoleColor::PrintError("Installation verification failed");
            return false;
        }
        return true;
    }

    inline bool ConnectOrLoadDriver(driver::DriverHandle& driver_handle, const VersionManager::VersionConfig& config) {
        if (driver_handle.is_valid()) {
            ConsoleColor::PrintSuccess("Driver already loaded and connected");
            Logger::LogSuccess("Driver already loaded and connected");
            return true;
        }
        ConsoleColor::PrintWarning("Driver not detected - loading...");
        Logger::LogWarning("Driver not detected - loading...");

        if (!WindowsSecurityHelper::GuideUserToDisableRealtimeProtection()) {
            ConsoleColor::PrintError("Cannot proceed without disabling Real-Time Protection");
            Logger::LogError("Cannot proceed without disabling Real-Time Protection");
            return false;
        }
        if (!EnsureDriverFilesPresent(config)) {
            return false;
        }
        if (!DriverLoaderHelper::LoadDriverWithKDU(config.deviceSymbolicLink)) {
            ConsoleColor::PrintError("Failed to load driver with KDU");
            Logger::LogError("Failed to load driver with KDU");
            return false;
        }
        const int maxConnectionAttempts = 5;
        for (int attempt = 1; attempt <= maxConnectionAttempts; attempt++) {
            ConsoleColor::PrintInfo("Connecting to driver (attempt " + std::to_string(attempt) + "/" + std::to_string(maxConnectionAttempts) + ")...");
            Logger::LogInfo("Connecting to driver (attempt " + std::to_string(attempt) + "/" + std::to_string(maxConnectionAttempts) + ")...");
            driver_handle.~DriverHandle();
            new (&driver_handle) driver::DriverHandle(config.deviceSymbolicLink);
            if (driver_handle.is_valid()) {
                ConsoleColor::PrintSuccess("Successfully connected to driver");
                Logger::LogSuccess("Successfully connected to driver on attempt " + std::to_string(attempt));
                return true;
            }
            if (attempt < maxConnectionAttempts) {
                int delayMs = attempt * 500;
                ConsoleColor::PrintInfo("Waiting " + std::to_string(delayMs) + "ms before retry...");
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        ConsoleColor::PrintError("Failed to connect to driver after " + std::to_string(maxConnectionAttempts) + " attempts");
        Logger::LogError("Failed to connect to driver after multiple attempts");
        return false;
    }
}
