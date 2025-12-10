#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "console_colors.hpp"
#include "download_helper.hpp"
#include "logger.hpp"

namespace DriverPatcher {

    // Convert ASCII string to byte pattern for searching
    inline std::vector<BYTE> StringToBytePattern(const std::string& str) {
        std::vector<BYTE> pattern;
        for (char c : str) {
            pattern.push_back(static_cast<BYTE>(c));
        }
        return pattern;
    }

    // Convert wide string to byte pattern (includes null bytes between chars)
    inline std::vector<BYTE> WideStringToBytePattern(const std::wstring& wstr) {
        std::vector<BYTE> pattern;
        for (wchar_t c : wstr) {
            pattern.push_back(static_cast<BYTE>(c & 0xFF));
            pattern.push_back(static_cast<BYTE>((c >> 8) & 0xFF));
        }
        return pattern;
    }

    // Find pattern in binary data and return all positions
    inline std::vector<size_t> FindPattern(const std::vector<BYTE>& data, const std::vector<BYTE>& pattern) {
        std::vector<size_t> positions;
        
        for (size_t i = 0; i <= data.size() - pattern.size(); i++) {
            if (memcmp(&data[i], pattern.data(), pattern.size()) == 0) {
                positions.push_back(i);
            }
        }
        
        return positions;
    }

    // Replace pattern at specific positions with new pattern (ensures proper null termination)
    inline void ReplaceAtPositions(std::vector<BYTE>& data, const std::vector<size_t>& positions, 
                                   const std::vector<BYTE>& oldPattern, const std::vector<BYTE>& newPattern) {
        for (size_t pos : positions) {
            // Calculate how much space we have
            size_t availableSpace = oldPattern.size();
            
            // Zero out the old pattern area (including extra bytes for safety)
            memset(&data[pos], 0, availableSpace);
            
            // Write new pattern (ensure it fits)
            size_t copySize = min(newPattern.size(), availableSpace);
            memcpy(&data[pos], newPattern.data(), copySize);
            
            ConsoleColor::SetColor(ConsoleColor::GREEN);
            std::cout << "[+] Replaced at offset 0x" << std::hex << pos << std::dec << "\n";
            ConsoleColor::Reset();
            
            // Log to file
            std::ostringstream oss;
            oss << "[+] Replaced at offset 0x" << std::hex << pos << std::dec;
            Logger::Log(oss.str());
        }
    }

    // Check if driver is already patched with target device name
    inline bool IsDriverAlreadyPatched(const std::string& sysFilePath, const std::string& targetDeviceName) {
        std::ifstream inputFile(sysFilePath, std::ios::binary | std::ios::ate);
        if (!inputFile.is_open()) {
            return false;
        }

        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<BYTE> fileData(fileSize);
        if (!inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
            inputFile.close();
            return false;
        }
        inputFile.close();

        // Check if target device name exists in driver
        std::string targetStr = "\\Device\\" + targetDeviceName;
        
        int wideLen = MultiByteToWideChar(CP_ACP, 0, targetStr.c_str(), -1, nullptr, 0);
        wchar_t* wide = new wchar_t[wideLen];
        MultiByteToWideChar(CP_ACP, 0, targetStr.c_str(), -1, wide, wideLen);
        
        std::vector<BYTE> pattern = WideStringToBytePattern(std::wstring(wide, wideLen - 1));
        delete[] wide;

        std::vector<size_t> positions = FindPattern(fileData, pattern);
        
        return !positions.empty();
    }

    // Patch the Hiroyoshi.sys file with a new device name
    inline bool PatchDriverDeviceName(const std::string& sysFilePath, const std::string& newDeviceName) {
        ConsoleColor::PrintInfo("Patching driver with custom device name...");
        ConsoleColor::SetColor(ConsoleColor::CYAN);
        std::cout << "[*] Target device name: " << newDeviceName << "\n";
        ConsoleColor::Reset();
        
        Logger::LogInfo("Patching driver with custom device name...");
        Logger::Log("[*] Target device name: " + newDeviceName);

        // Read the entire .sys file into memory
        std::ifstream inputFile(sysFilePath, std::ios::binary | std::ios::ate);
        if (!inputFile.is_open()) {
            ConsoleColor::PrintError("Failed to open driver file: " + sysFilePath);
            Logger::LogError("Failed to open driver file: " + sysFilePath);
            return false;
        }

        std::streamsize fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        std::vector<BYTE> fileData(fileSize);
        if (!inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
            ConsoleColor::PrintError("Failed to read driver file");
            Logger::LogError("Failed to read driver file");
            inputFile.close();
            return false;
        }
        inputFile.close();

        ConsoleColor::PrintSuccess("Driver file loaded (" + std::to_string(fileSize) + " bytes)");
        Logger::LogSuccess("Driver file loaded (" + std::to_string(fileSize) + " bytes)");

        // Build search/replace patterns as ASCII strings (the driver uses ASCII strings in RtlInitUnicodeString)
        // The driver source uses: RtlInitUnicodeString(&deviceName, L"\\Device\\airway_radio");
        // This stores the WIDE string as UTF-16LE in the binary
        
        std::vector<std::pair<std::string, std::string>> patchPairs = {
            { "\\Device\\airway_radio", "\\Device\\" + newDeviceName },
            { "\\DosDevices\\airway_radio", "\\DosDevices\\" + newDeviceName },
            { "\\Driver\\airway_radio", "\\Driver\\" + newDeviceName }
        };

        int totalPatches = 0;
        bool foundAnyPattern = false;

        for (const auto& [oldStr, newStr] : patchPairs) {
            ConsoleColor::SetColor(ConsoleColor::CYAN);
            std::cout << "[*] Searching for: " << oldStr << "\n";
            ConsoleColor::Reset();
            
            Logger::Log("[*] Searching for: " + oldStr);

            // Convert to wide string byte patterns (UTF-16LE)
            int oldWideLen = MultiByteToWideChar(CP_ACP, 0, oldStr.c_str(), -1, nullptr, 0);
            int newWideLen = MultiByteToWideChar(CP_ACP, 0, newStr.c_str(), -1, nullptr, 0);
            
            wchar_t* oldWide = new wchar_t[oldWideLen];
            wchar_t* newWide = new wchar_t[newWideLen];
            
            MultiByteToWideChar(CP_ACP, 0, oldStr.c_str(), -1, oldWide, oldWideLen);
            MultiByteToWideChar(CP_ACP, 0, newStr.c_str(), -1, newWide, newWideLen);
            
            // Create byte patterns (without the null terminator)
            std::vector<BYTE> oldPattern = WideStringToBytePattern(std::wstring(oldWide, oldWideLen - 1));
            std::vector<BYTE> newPattern = WideStringToBytePattern(std::wstring(newWide, newWideLen - 1));
            
            delete[] oldWide;
            delete[] newWide;

            // Search for pattern
            std::vector<size_t> positions = FindPattern(fileData, oldPattern);
            
            if (!positions.empty()) {
                foundAnyPattern = true;
                ConsoleColor::SetColor(ConsoleColor::GREEN);
                std::cout << "[+] Found " << positions.size() << " occurrence(s)\n";
                ConsoleColor::Reset();
                
                Logger::LogSuccess("Found " + std::to_string(positions.size()) + " occurrence(s)");

                // Check if new string fits in the space
                if (newPattern.size() > oldPattern.size()) {
                    ConsoleColor::PrintError("New device name is too long! Max length: " + std::to_string(oldPattern.size() / 2) + " chars");
                    ConsoleColor::PrintError("Your name: " + newStr + " (" + std::to_string(newPattern.size() / 2) + " chars)");
                    Logger::LogError("New device name is too long! Max length: " + std::to_string(oldPattern.size() / 2) + " chars");
                    Logger::LogError("Your name: " + newStr + " (" + std::to_string(newPattern.size() / 2) + " chars)");
                    return false;
                }

                // Replace all occurrences
                ReplaceAtPositions(fileData, positions, oldPattern, newPattern);
                totalPatches += static_cast<int>(positions.size());
                
                ConsoleColor::SetColor(ConsoleColor::GREEN);
                std::cout << "[+] Replaced with: " << newStr << "\n";
                ConsoleColor::Reset();
                
                Logger::LogSuccess("Replaced with: " + newStr);
            } else {
                ConsoleColor::SetColor(ConsoleColor::YELLOW);
                std::cout << "[!] Pattern not found: " << oldStr << "\n";
                ConsoleColor::Reset();
                
                Logger::LogWarning("Pattern not found: " + oldStr);
            }
        }

        if (!foundAnyPattern) {
            ConsoleColor::PrintError("CRITICAL: No patterns found in driver binary!");
            ConsoleColor::PrintError("The driver may be corrupted or incompatible");
            ConsoleColor::PrintInfo("Attempting to restore from backup...");
            
            Logger::LogError("CRITICAL: No patterns found in driver binary!");
            Logger::LogError("The driver may be corrupted or incompatible");
            
            // Try to restore from backup
            std::string backupPath = sysFilePath + ".original";
            if (DownloadHelper::FileExists(backupPath)) {
                std::ifstream backup(backupPath, std::ios::binary);
                std::ofstream output(sysFilePath, std::ios::binary | std::ios::trunc);
                if (backup && output) {
                    output << backup.rdbuf();
                    ConsoleColor::PrintSuccess("Restored from backup");
                    Logger::LogSuccess("Restored from backup");
                }
            }
            
            return false;
        }

        ConsoleColor::PrintSuccess("Successfully patched " + std::to_string(totalPatches) + " occurrence(s)");
        Logger::LogSuccess("Successfully patched " + std::to_string(totalPatches) + " occurrence(s)");

        // Create backup of original driver (only if not already exists)
        std::string backupPath = sysFilePath + ".original";
        if (!DownloadHelper::FileExists(backupPath)) {
            // Restore original and create backup
            ConsoleColor::PrintInfo("Creating backup of original driver...");
            
            // We need to restore the original first, so read it from the download
            // For now, just warn the user
            ConsoleColor::PrintWarning("Note: Backup should be created before first patch");
        }

        // Write the patched file back
        std::ofstream outputFile(sysFilePath, std::ios::binary | std::ios::trunc);
        if (!outputFile.is_open()) {
            ConsoleColor::PrintError("Failed to write patched driver file");
            Logger::LogError("Failed to write patched driver file");
            return false;
        }

        outputFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        outputFile.close();

        ConsoleColor::PrintSuccess("Driver file patched and saved successfully!");
        Logger::LogSuccess("Driver file patched and saved successfully!");
        
        return true;
    }

    // Create backup before any patching
    inline bool CreateDriverBackup(const std::string& sysFilePath) {
        std::string backupPath = sysFilePath + ".original";
        
        // Don't overwrite existing backup
        if (DownloadHelper::FileExists(backupPath)) {
            return true;
        }

        std::ifstream source(sysFilePath, std::ios::binary);
        std::ofstream dest(backupPath, std::ios::binary);
        
        if (!source || !dest) {
            return false;
        }

        dest << source.rdbuf();
        return true;
    }

    // Restore original driver from backup
    inline bool RestoreOriginalDriver(const std::string& sysFilePath) {
        std::string backupPath = sysFilePath + ".original";
        
        if (!DownloadHelper::FileExists(backupPath)) {
            ConsoleColor::PrintWarning("No backup found to restore");
            return false;
        }

        std::ifstream backupFile(backupPath, std::ios::binary);
        std::ofstream outputFile(sysFilePath, std::ios::binary | std::ios::trunc);
        
        if (!backupFile || !outputFile) {
            ConsoleColor::PrintError("Failed to restore from backup");
            Logger::LogError("Failed to restore from backup");
            return false;
        }

        outputFile << backupFile.rdbuf();
        ConsoleColor::PrintSuccess("Original driver restored from backup");
        Logger::LogSuccess("Original driver restored from backup");
        return true;
    }
}
