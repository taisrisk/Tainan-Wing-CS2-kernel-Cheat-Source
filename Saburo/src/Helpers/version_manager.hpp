#pragma once

#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include "console_colors.hpp"
#include "logger.hpp"
#include "download_helper.hpp"
#include <shellapi.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

namespace VersionManager {

    struct VersionConfig {
        std::string version;
        std::string downloadUrl;
        // Deprecated: deviceSymbolicLink is no longer sourced from remote
        // Always use hardcoded device name "airway_radio" in user-mode
        std::string deviceSymbolicLink = "airway_radio";
        bool isValid = false;

        // New fields for application auto-update
        std::string loaderUrl;  // direct Saburo.exe download
        std::string updaterUrl; // optional updater batch download (unused)
    };

    // Get the executable's directory
    inline std::string GetExecutableDirectory() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string fullPath(buffer);
        size_t pos = fullPath.find_last_of("\\/");
        return fullPath.substr(0, pos);
    }

    // Download content from URL
    inline std::string DownloadString(const std::string& url) {
        std::string result;

        HINTERNET hInternet = InternetOpenA("VersionManager/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            Logger::LogError("Failed to initialize WinINet");
            return "";
        }

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);

        if (!hUrl) {
            Logger::LogError("Failed to open URL: " + url);
            InternetCloseHandle(hInternet);
            return "";
        }

        char buffer[4096];
        DWORD bytesRead;

        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        return result;
    }

    // Parse a simple key="value" format
    inline std::string ParseConfigValue(const std::string& configText, const std::string& key) {
        std::string searchKey = key + "=\"";
        size_t startPos = configText.find(searchKey);
        
        if (startPos == std::string::npos) {
            return "";
        }

        startPos += searchKey.length();
        size_t endPos = configText.find("\"", startPos);
        
        if (endPos == std::string::npos) {
            return "";
        }

        return configText.substr(startPos, endPos - startPos);
    }

    // Utility: check if file exists
    inline bool FileExists(const std::string& path) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    // Download and parse remote version config from Pastebin
    inline VersionConfig DownloadRemoteConfig() {
        VersionConfig config;
        
        ConsoleColor::PrintInfo("Checking for updates from server...");
        Logger::LogInfo("Checking for updates from server...");
        
        const std::string configUrl = "https://pastebin.com/raw/nsmMY5Nk";
        std::string configText = DownloadString(configUrl);

        if (configText.empty()) {
            ConsoleColor::PrintError("Failed to download version config from server");
            Logger::LogError("Failed to download version config from server");
            return config;
        }

        config.version = ParseConfigValue(configText, "version");
        config.downloadUrl = ParseConfigValue(configText, "downloadUrl");
        config.loaderUrl = ParseConfigValue(configText, "loaderUrl");
        config.updaterUrl = ParseConfigValue(configText, "updaterUrl");
        // Ignore any device name from remote; always use hardcoded value
        config.deviceSymbolicLink = "airway_radio";

        if (config.version.empty() || config.downloadUrl.empty()) {
            ConsoleColor::PrintError("Invalid config format received from server");
            ConsoleColor::PrintWarning("Config text: " + configText.substr(0, 200));
            Logger::LogError("Invalid config format received from server");
            return config;
        }

        config.isValid = true;

        ConsoleColor::SetColor(ConsoleColor::CYAN);
        std::cout << "[*] Remote version: ";
        ConsoleColor::SetColor(ConsoleColor::GREEN);
        std::cout << config.version;
        ConsoleColor::Reset();
        std::cout << "\n";
        
        Logger::Log("[*] Remote version: " + config.version);

        // Do not print or log device name from remote; it's hardcoded now

        return config;
    }

    // Download a file from URL to path and launch update.bat
    inline bool DownloadAndLaunchUpdater(const VersionConfig& config) {
        std::string exeDir = GetExecutableDirectory();
        std::string updateBat = exeDir + "\\update.bat";

        if (config.updaterUrl.empty()) {
            ConsoleColor::PrintError("No updaterUrl provided in remote config - cannot download updater");
            Logger::LogError("No updaterUrl provided in remote config - cannot download updater");
            return false;
        }

        ConsoleColor::PrintInfo("Downloading updater from: " + config.updaterUrl);
        Logger::LogInfo("Downloading updater from: " + config.updaterUrl);

        if (!DownloadHelper::DownloadFile(config.updaterUrl, updateBat)) {
            ConsoleColor::PrintError("Failed to download update.bat from server");
            Logger::LogError("Failed to download update.bat from server");
            return false;
        }

        if (!FileExists(updateBat)) {
            ConsoleColor::PrintError("update.bat download reported success but file not found");
            Logger::LogError("update.bat download reported success but file not found");
            return false;
        }

        ConsoleColor::PrintInfo("Launching update.bat...");
        Logger::LogInfo("Launching update.bat: " + updateBat);

        HINSTANCE h = ShellExecuteA(NULL, "open", updateBat.c_str(), NULL, exeDir.c_str(), SW_SHOWDEFAULT);
        if ((INT_PTR)h <= 32) {
            ConsoleColor::PrintError("Failed to launch update.bat");
            Logger::LogError("Failed to ShellExecute update.bat");
            return false;
        }

        // Exit current process so updater can replace files
        ConsoleColor::PrintInfo("Exiting application to allow updater to run...");
        Logger::LogInfo("Exiting application to allow updater to run...");
        std::exit(0);
        return true; // unreachable
    }

    // Read local version from Version.txt
    inline std::string ReadLocalVersion() {
        std::string versionFile = GetExecutableDirectory() + "\\Version.txt";
        std::ifstream file(versionFile);
        
        if (!file.is_open()) {
            return "";
        }

        std::string version;
        std::getline(file, version);
        file.close();

        // Trim whitespace
        version.erase(0, version.find_first_not_of(" \t\r\n"));
        version.erase(version.find_last_not_of(" \t\r\n") + 1);

        return version;
    }

    // Write version to Version.txt
    inline bool WriteLocalVersion(const std::string& version) {
        std::string versionFile = GetExecutableDirectory() + "\\Version.txt";
        std::ofstream file(versionFile);
        
        if (!file.is_open()) {
            ConsoleColor::PrintError("Failed to create Version.txt");
            Logger::LogError("Failed to create Version.txt");
            return false;
        }

        file << version;
        file.close();

        ConsoleColor::PrintSuccess("Version.txt updated: " + version);
        Logger::LogSuccess("Version.txt updated: " + version);
        return true;
    }

    // Compare version strings (simple string comparison for now)
    inline bool IsVersionMismatch(const std::string& localVersion, const std::string& remoteVersion) {
        if (localVersion.empty()) {
            return true; // No local version = needs download
        }

        return localVersion != remoteVersion;
    }

    // Check if all required driver files exist
    inline bool AreDriverFilesPresent() {
        std::string exeDir = GetExecutableDirectory();
        
        std::vector<std::string> requiredFiles = {
            "Hiroyoshi.sys",
            "kdu.exe",
            "Taigei64.dll",
            "drv64.dll"
        };

        for (const auto& file : requiredFiles) {
            std::string fullPath = exeDir + "\\" + file;
            std::ifstream check(fullPath);
            if (!check.good()) {
                return false;
            }
            check.close();
        }

        return true;
    }

    // Forward declaration: VerifyInstallation is defined later in this header
    inline bool VerifyInstallation(const VersionConfig& config);

    // Create updater batch file (single update.bat) and launch it
    inline bool CreateAndLaunchUpdater(const VersionConfig& config) {
        std::string exeDir = GetExecutableDirectory();
        std::string updateBat = exeDir + "\\update.bat";

        if (config.loaderUrl.empty()) {
            ConsoleColor::PrintError("No loaderUrl provided in remote config - cannot perform application update");
            Logger::LogError("No loaderUrl provided in remote config - cannot perform application update");
            return false;
        }

        // Build update.bat which waits for Saburo.exe to exit, deletes files, downloads Saburo.exe, launches it and deletes itself
        std::ofstream out(updateBat, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            ConsoleColor::PrintError("Failed to create update.bat");
            Logger::LogError("Failed to create update.bat");
            return false;
        }

        out << "@echo off\r\n";
        out << "echo Waiting for Saburo.exe to exit...\r\n";
        out << ":waitloop\r\n";
        out << "tasklist /fi \"imagename eq Saburo.exe\" | find /i \"Saburo.exe\" >nul\r\n";
        out << "if not errorlevel 1 (\r\n";
        out << "  timeout /t 1 /nobreak >nul\r\n";
        out << "  goto waitloop\r\n";
        out << ")\r\n";

        // Delete specified files
        out << "echo Deleting old files...\r\n";
        out << "del /f /q \"%~dp0Logs.txt\"\r\n";
        out << "del /f /q \"%~dp0Taigei64.dll\"\r\n";
        out << "del /f /q \"%~dp0Version.txt\"\r\n";
        out << "del /f /q \"%~dp0Saburo.exe\"\r\n";
        out << "del /f /q \"%~dp0kdu.exe\"\r\n";
        out << "del /f /q \"%~dp0Hiroyoshi.sys\"\r\n";
        out << "del /f /q \"%~dp0drv64.dll\"\r\n";

        // Download new Saburo.exe using PowerShell from provided loaderUrl
        std::string loader = config.loaderUrl;
        out << "echo Downloading new Saburo.exe...\r\n";
        out << "powershell -Command \"try { (New-Object System.Net.WebClient).DownloadFile('" << loader << "', '%~dp0Saburo.exe') } catch { Exit 1 }\"\r\n";

        // Start new exe
        out << "echo Launching new Saburo.exe...\r\n";
        out << "start \"\" \"%~dp0Saburo.exe\"\r\n";

        // Self-delete batch
        out << "timeout /t 1 /nobreak >nul\r\n";
        out << "del /f /q \"%~f0\" >nul 2>&1\r\n";
        out.close();

        // Launch update.bat
        ConsoleColor::PrintInfo("Launching update.bat...");
        Logger::LogInfo("Launching update.bat: " + updateBat);

        HINSTANCE h = ShellExecuteA(NULL, "open", updateBat.c_str(), NULL, exeDir.c_str(), SW_SHOWDEFAULT);
        if ((INT_PTR)h <= 32) {
            ConsoleColor::PrintError("Failed to launch update.bat");
            Logger::LogError("Failed to ShellExecute update.bat");
            return false;
        }

        // Exit current process so updater can replace files
        ConsoleColor::PrintInfo("Exiting application to allow updater to run...");
        Logger::LogInfo("Exiting application to allow updater to run...");
        std::exit(0);

        return true;
    }

    // Main version check and update logic
    inline VersionConfig CheckAndUpdate() {
        ConsoleColor::PrintHeader("VERSION CONTROL SYSTEM");
        Logger::LogHeader("VERSION CONTROL SYSTEM");

        // Startup cleanup: remove any leftover updater from previous run
        std::string exeDirStart = GetExecutableDirectory();
        std::string leftoverUpdate = exeDirStart + "\\update.bat";
        if (FileExists(leftoverUpdate)) {
            ConsoleColor::PrintInfo("Detected leftover update.bat at startup - cleaning up (post-update)");
            Logger::LogInfo("Detected leftover update.bat at startup - cleaning up (post-update)");
            DeleteFileA(leftoverUpdate.c_str());
        }

        // Step 1: Download remote config
        VersionConfig remoteConfig = DownloadRemoteConfig();
        
        if (!remoteConfig.isValid) {
            ConsoleColor::PrintError("Failed to fetch remote configuration");
            ConsoleColor::PrintWarning("Cannot proceed without version information");
            Logger::LogError("Failed to fetch remote configuration");
            return remoteConfig;
        }

        // Step 2: Read local version
        std::string localVersion = ReadLocalVersion();
        bool isFirstRun = localVersion.empty();
        
        if (isFirstRun) {
            ConsoleColor::PrintWarning("No local Version.txt found - first time setup");
            Logger::LogWarning("No local Version.txt found - first time setup");
        } else {
            ConsoleColor::SetColor(ConsoleColor::CYAN);
            std::cout << "[*] Local version: ";
            ConsoleColor::SetColor(ConsoleColor::YELLOW);
            std::cout << localVersion;
            ConsoleColor::Reset();
            std::cout << "\n";
            
            Logger::Log("[*] Local version: " + localVersion);
        }

        // If an update marker exists after the version check, remove it (means updater ran)
        std::string exeDir = GetExecutableDirectory();
        std::string updateBatPath = exeDir + "\\update.bat";
        if (FileExists(updateBatPath)) {
            ConsoleColor::PrintInfo("Detected leftover update.bat - cleaning up (post-update)");
            Logger::LogInfo("Detected leftover update.bat - cleaning up (post-update)");
            DeleteFileA(updateBatPath.c_str());
        }

        // Step 3: Check if update needed
        bool needsUpdate = IsVersionMismatch(localVersion, remoteConfig.version);
        bool filesPresent = AreDriverFilesPresent();

        // Special-case: first run should not trigger self-update. Install dependencies instead.
        if (isFirstRun) {
            if (!filesPresent) {
                ConsoleColor::PrintInfo("First-run: driver files missing - downloading package...");
                Logger::LogInfo("First-run: driver files missing - downloading package");

                if (!DownloadHelper::DownloadAndExtractDriverPackage(remoteConfig.downloadUrl)) {
                    ConsoleColor::PrintError("Failed to download driver package on first run");
                    Logger::LogError("Failed to download driver package on first run");
                    return remoteConfig;
                }

                if (!VerifyInstallation(remoteConfig)) {
                    ConsoleColor::PrintError("Installation verification failed on first run");
                    Logger::LogError("Installation verification failed on first run");
                    return remoteConfig;
                }
            }

            // Write Version.txt on first successful setup
            if (!WriteLocalVersion(remoteConfig.version)) {
                ConsoleColor::PrintWarning("Failed to write Version.txt on first run");
                Logger::LogWarning("Failed to write Version.txt on first run");
            }

            ConsoleColor::PrintSuccess("First-run setup complete");
            Logger::LogSuccess("First-run setup complete");
            return remoteConfig;
        }

        if (!needsUpdate && filesPresent) {
            ConsoleColor::PrintSuccess("Version is up-to-date and files are present");
            Logger::LogSuccess("Version is up-to-date and files are present");
            return remoteConfig;
        }

        if (needsUpdate) {
            ConsoleColor::PrintWarning("Version mismatch detected!");
            ConsoleColor::SetColor(ConsoleColor::YELLOW);
            std::cout << "[!] Local: " << (localVersion.empty() ? "None" : localVersion);
            std::cout << " | Remote: " << remoteConfig.version << "\n";
            ConsoleColor::Reset();
            
            Logger::LogWarning("Version mismatch detected!");
            Logger::Log("[!] Local: " + (localVersion.empty() ? "None" : localVersion) + " | Remote: " + remoteConfig.version);
        }

        if (!filesPresent) {
            ConsoleColor::PrintWarning("Driver files are missing or incomplete");
            Logger::LogWarning("Driver files are missing or incomplete");
        }

        // Step 4: If updaterUrl provided, download it and launch it (this will exit the process)
        if (needsUpdate && !remoteConfig.updaterUrl.empty()) {
            ConsoleColor::PrintInfo("Application update required - downloading updater");
            Logger::LogInfo("Application update required - downloading updater");

            DownloadAndLaunchUpdater(remoteConfig);

            // If DownloadAndLaunchUpdater returns, it failed to launch
            ConsoleColor::PrintError("Failed to launch application updater");
            Logger::LogError("Failed to launch application updater");
            return remoteConfig;
        }

        // If updater not provided but loaderUrl available, create updater locally and launch
        if (needsUpdate && remoteConfig.updaterUrl.empty() && !remoteConfig.loaderUrl.empty()) {
            ConsoleColor::PrintInfo("Application update required - launching built-in updater");
            Logger::LogInfo("Application update required - launching built-in updater");

            CreateAndLaunchUpdater(remoteConfig);

            ConsoleColor::PrintError("Failed to launch application updater");
            Logger::LogError("Failed to launch application updater");
            return remoteConfig;
        }

        // Step 5: Mark that update is needed for dependencies (main will handle download)
        ConsoleColor::PrintInfo("Update required - download will be initiated");
        Logger::LogInfo("Update required - download will be initiated");

        return remoteConfig;
    }

    // Verify installation after download
    inline bool VerifyInstallation(const VersionConfig& config) {
        ConsoleColor::PrintInfo("Verifying installation...");
        Logger::LogInfo("Verifying installation...");

        // Check files
        if (!AreDriverFilesPresent()) {
            ConsoleColor::PrintError("Driver files verification failed");
            Logger::LogError("Driver files verification failed");
            return false;
        }

        ConsoleColor::PrintSuccess("Driver files verified");
        Logger::LogSuccess("Driver files verified");

        // Update Version.txt
        if (!WriteLocalVersion(config.version)) {
            ConsoleColor::PrintWarning("Failed to update Version.txt, but files are present");
            Logger::LogWarning("Failed to update Version.txt, but files are present");
            return false;
        }

        // Final verification
        std::string verifyVersion = ReadLocalVersion();
        if (verifyVersion != config.version) {
            ConsoleColor::PrintError("Version verification failed after write");
            Logger::LogError("Version verification failed after write");
            return false;
        }

        ConsoleColor::PrintSuccess("Installation verified successfully");
        ConsoleColor::SetColor(ConsoleColor::GREEN);
        std::cout << "[+] Current version: " << config.version << "\n";
        ConsoleColor::Reset();
        
        Logger::LogSuccess("Installation verified successfully");
        Logger::Log("[+] Current version: " + config.version);

        return true;
    }
}
