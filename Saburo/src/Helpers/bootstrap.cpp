#include "bootstrap.hpp"
#include "download_helper.hpp"
#include "driver_loader_helper.hpp"
#include "windows_security_helper.hpp"
#include "version_manager.hpp"
#include "console_logger.hpp"
#include "../Core/process.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace Bootstrap {

bool InitializeLogger() {
    if (!Logger::Initialize()) {
        ConsoleColor::PrintWarning("Failed to initialize logger - proceeding without file logging");
        return false;
    }
    ConsoleColor::PrintSuccess("Logger initialized - session logged to Logs.txt");
    ConsoleLogger::setEnabled(false);
    return true;
}

bool VersionCheckAndEnsureFiles(VersionManager::VersionConfig& outConfig) {
    ConsoleColor::PrintHeader("SUBARO INITIALIZING");
    Logger::LogHeader("SUBARO INITIALIZING");

    outConfig = VersionManager::CheckAndUpdate();
    if (!outConfig.isValid) {
        ConsoleColor::PrintError("Failed to fetch version configuration");
        return false;
    }

    std::string localVersion = VersionManager::ReadLocalVersion();
    bool needsUpdate = VersionManager::IsVersionMismatch(localVersion, outConfig.version);
    bool filesPresent = VersionManager::AreDriverFilesPresent();

    if (needsUpdate || !filesPresent) {
        ConsoleColor::PrintInfo("Checking Windows Security (Real-Time Protection)...");
        if (!WindowsSecurityHelper::GuideUserToDisableRealtimeProtection()) {
            ConsoleColor::PrintError("Cannot proceed while Real-Time Protection is enabled");
            return false;
        }

        ConsoleColor::PrintInfo("Downloading driver package...");
        if (!DownloadHelper::DownloadAndExtractDriverPackage(outConfig.downloadUrl)) {
            ConsoleColor::PrintError("Failed to download driver package");
            return false;
        }

        if (!VersionManager::VerifyInstallation(outConfig)) {
            ConsoleColor::PrintError("Installation verification failed");
            return false;
        }
        ConsoleColor::PrintSuccess("Driver package updated");
    } else {
        ConsoleColor::PrintSuccess("Version check passed");
    }
    return true;
}

bool ConnectOrLoadDriver(driver::DriverHandle& driver_handle, const VersionManager::VersionConfig& cfg) {
    if (driver_handle.is_valid()) {
        ConsoleColor::PrintSuccess("Driver already loaded and connected");
        Logger::LogSuccess("Driver already loaded and connected");
        return true;
    }

    ConsoleColor::PrintWarning("Driver not detected - loading...");
    Logger::LogWarning("Driver not detected - loading...");

    if (!WindowsSecurityHelper::GuideUserToDisableRealtimeProtection()) {
        ConsoleColor::PrintError("Cannot proceed without disabling Real-Time Protection");
        return false;
    }

    // Files should be present now from VersionCheckAndEnsureFiles
    if (!DriverLoaderHelper::LoadDriverWithKDU("airway_radio")) {
        ConsoleColor::PrintError("Failed to load driver with KDU");
        return false;
    }

    const int maxConnectionAttempts = 5;
    for (int attempt = 1; attempt <= maxConnectionAttempts; attempt++) {
        ConsoleColor::PrintInfo("Connecting to driver (attempt " + std::to_string(attempt) + "/" + std::to_string(maxConnectionAttempts) + ")...");
        driver_handle.~DriverHandle();
        new (&driver_handle) driver::DriverHandle();
        if (driver_handle.is_valid()) {
            ConsoleColor::PrintSuccess("Successfully connected to driver");
            Logger::LogSuccess("Connected to driver on attempt " + std::to_string(attempt));
            return true;
        }
        if (attempt < maxConnectionAttempts)
            std::this_thread::sleep_for(std::chrono::milliseconds(attempt * 500));
    }

    ConsoleColor::PrintError("Failed to connect to driver after retries");
    return false;
}

DWORD WaitForProcess(const std::wstring& name) {
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

void StartProcessMonitor(std::atomic<bool>& runningFlag, std::atomic<bool>& shutdownFlag, DWORD& pidRef) {
    std::thread([&]() {
        while (!shutdownFlag.load()) {
            if (pidRef != 0) {
                DWORD pid = process::get_process_id_by_name(L"cs2.exe");
                if (pid == 0) {
                    runningFlag.store(false);
                    ConsoleColor::PrintWarning("CS2 process terminated!");
                    Logger::LogWarning("CS2 process terminated - PID " + std::to_string(pidRef) + " no longer exists");
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }).detach();
}

bool AttachToProcess(driver::DriverHandle& driver, DWORD pid) {
    if (!driver.attach_to_process(reinterpret_cast<HANDLE>(static_cast<uint64_t>(pid)))) {
        ConsoleColor::PrintError("Attach failed");
        return false;
    }
    ConsoleColor::PrintSuccess("Attached to CS2");
    return true;
}

bool WaitForClientDll(driver::DriverHandle& driver, DWORD pid, std::uintptr_t& outClientBase) {
    ConsoleColor::PrintInfo("Waiting for client.dll...");
    int attempts = 0;
    while (attempts < 100) {
        outClientBase = driver.get_module_base(pid, L"client.dll");
        if (outClientBase != 0) {
            ConsoleColor::PrintSuccess("client.dll found");
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        attempts++;
    }
    ConsoleColor::PrintError("client.dll not found");
    return false;
}

} // namespace Bootstrap
