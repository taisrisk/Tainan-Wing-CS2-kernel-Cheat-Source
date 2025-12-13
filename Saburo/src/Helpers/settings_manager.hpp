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
        // Advanced toggles
        bool boneESPEnabled = false;
        bool silentAimEnabled = false;
        bool thirdPersonEnabled = false;
        bool recoilCompEnabled = false;
        bool smoothAimEnabled = false;
        bool entityPredictorEnabled = false;
        bool visibilityCheckEnabled = false;
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
            file << "boneESPEnabled=" << settings.boneESPEnabled << "\n";
            file << "silentAimEnabled=" << settings.silentAimEnabled << "\n";
            file << "thirdPersonEnabled=" << settings.thirdPersonEnabled << "\n";
            file << "recoilCompEnabled=" << settings.recoilCompEnabled << "\n";
            file << "smoothAimEnabled=" << settings.smoothAimEnabled << "\n";
            file << "entityPredictorEnabled=" << settings.entityPredictorEnabled << "\n";
            file << "visibilityCheckEnabled=" << settings.visibilityCheckEnabled << "\n";

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
                else if (key == "boneESPEnabled") settings.boneESPEnabled = boolValue;
                else if (key == "silentAimEnabled") settings.silentAimEnabled = boolValue;
                else if (key == "thirdPersonEnabled") settings.thirdPersonEnabled = boolValue;
                else if (key == "recoilCompEnabled") settings.recoilCompEnabled = boolValue;
                else if (key == "smoothAimEnabled") settings.smoothAimEnabled = boolValue;
                else if (key == "entityPredictorEnabled") settings.entityPredictorEnabled = boolValue;
                else if (key == "visibilityCheckEnabled") settings.visibilityCheckEnabled = boolValue;
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
