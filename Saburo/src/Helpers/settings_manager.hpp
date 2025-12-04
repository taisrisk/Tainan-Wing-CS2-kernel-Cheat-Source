#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace SettingsManager {
    
    struct Settings {
        bool triggerbotEnabled = true;
        bool bhopEnabled = false;
        bool aimbotEnabled = false;
        bool consoleDebugEnabled = false;
        bool teamCheckEnabled = true;
        bool headAngleLineEnabled = false;
        bool snaplinesEnabled = false;
        bool distanceESPEnabled = false;
        bool snaplinesWallCheckEnabled = false;
        bool chamsEnabled = false;
    };

    // Get settings file path
    inline std::string GetSettingsFilePath() {
        return "saburo_settings.cfg";
    }

    // Save settings to file
    inline bool SaveSettings(const Settings& settings) {
        try {
            std::ofstream file(GetSettingsFilePath());
            if (!file.is_open()) {
                return false;
            }

            file << "triggerbotEnabled=" << settings.triggerbotEnabled << "\n";
            file << "bhopEnabled=" << settings.bhopEnabled << "\n";
            file << "aimbotEnabled=" << settings.aimbotEnabled << "\n";
            file << "consoleDebugEnabled=" << settings.consoleDebugEnabled << "\n";
            file << "teamCheckEnabled=" << settings.teamCheckEnabled << "\n";
            file << "headAngleLineEnabled=" << settings.headAngleLineEnabled << "\n";
            file << "snaplinesEnabled=" << settings.snaplinesEnabled << "\n";
            file << "distanceESPEnabled=" << settings.distanceESPEnabled << "\n";
            file << "snaplinesWallCheckEnabled=" << settings.snaplinesWallCheckEnabled << "\n";
            file << "chamsEnabled=" << settings.chamsEnabled << "\n";

            file.close();
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // Load settings from file
    inline Settings LoadSettings() {
        Settings settings; // Default values
        
        try {
            std::ifstream file(GetSettingsFilePath());
            if (!file.is_open()) {
                return settings; // Return defaults if file doesn't exist
            }

            std::string line;
            while (std::getline(file, line)) {
                size_t pos = line.find('=');
                if (pos == std::string::npos) continue;

                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                bool boolValue = (value == "1");

                if (key == "triggerbotEnabled") settings.triggerbotEnabled = boolValue;
                else if (key == "bhopEnabled") settings.bhopEnabled = boolValue;
                else if (key == "aimbotEnabled") settings.aimbotEnabled = boolValue;
                else if (key == "consoleDebugEnabled") settings.consoleDebugEnabled = boolValue;
                else if (key == "teamCheckEnabled") settings.teamCheckEnabled = boolValue;
                else if (key == "headAngleLineEnabled") settings.headAngleLineEnabled = boolValue;
                else if (key == "snaplinesEnabled") settings.snaplinesEnabled = boolValue;
                else if (key == "distanceESPEnabled") settings.distanceESPEnabled = boolValue;
                else if (key == "snaplinesWallCheckEnabled") settings.snaplinesWallCheckEnabled = boolValue;
                else if (key == "chamsEnabled") settings.chamsEnabled = boolValue;
            }

            file.close();
        }
        catch (...) {
            // Return defaults on error
        }

        return settings;
    }

    // Check if settings file exists
    inline bool SettingsFileExists() {
        return std::filesystem::exists(GetSettingsFilePath());
    }
}
