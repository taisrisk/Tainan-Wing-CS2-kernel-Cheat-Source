#pragma once

#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include "console_colors.hpp"
#include "logger.hpp"

#pragma comment(lib, "wininet.lib")

namespace VersionManager {

    struct VersionConfig {
        std::string version;
        std::string downloadUrl;
        std::string deviceSymbolicLink;
        bool isValid = false;
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
        config.deviceSymbolicLink = ParseConfigValue(configText, "device_symbollink");

        if (config.version.empty() || config.downloadUrl.empty() || config.deviceSymbolicLink.empty()) {
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

        ConsoleColor::SetColor(ConsoleColor::CYAN);
        std::cout << "[*] Device name: ";
        ConsoleColor::SetColor(ConsoleColor::YELLOW);
        std::cout << config.deviceSymbolicLink;
        ConsoleColor::Reset();
        std::cout << "\n";
        
        Logger::Log("[*] Device name: " + config.deviceSymbolicLink);

        return config;
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

    // Main version check and update logic
    inline VersionConfig CheckAndUpdate() {
        ConsoleColor::PrintHeader("VERSION CONTROL SYSTEM");
        Logger::LogHeader("VERSION CONTROL SYSTEM");

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
        
        if (localVersion.empty()) {
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

        // Step 3: Check if update needed
        bool needsUpdate = IsVersionMismatch(localVersion, remoteConfig.version);
        bool filesPresent = AreDriverFilesPresent();

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

        // Step 4: Mark that update is needed (main.cpp will handle download)
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
