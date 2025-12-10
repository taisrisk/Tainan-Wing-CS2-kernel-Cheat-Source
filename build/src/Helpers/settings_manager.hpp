#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace SettingsManager {
    
    struct Settings {
        // Core Features (Keys 0-9)
        bool consoleDebugEnabled = false;      // Key 0
        bool aimbotEnabled = false;            // Key 1
        bool triggerbotEnabled = true;         // Key 2
        bool bhopEnabled = false;              // Key 3
        bool headAngleLineEnabled = false;     // Key 4
        bool snaplinesEnabled = false;         // Key 5
        bool distanceESPEnabled = false;       // Key 6
        bool snaplinesWallCheckEnabled = false;// Key 7
        bool teamCheckEnabled = true;          // Key 8
        bool chamsEnabled = false;             // Key 9
        
        // Extended Features (F-Keys)
        bool boneESPEnabled = false;           // F1
        bool silentAimEnabled = false;         // F2
        bool thirdPersonEnabled = false;       // F3
        bool recoilCompEnabled = false;        // F4
        bool smoothAimEnabled = false;         // F5
        bool entityPredictorEnabled = false;   // F6
        bool visibilityCheckEnabled = false;   // F7
        
        // Feature Parameters
        float smoothAimFactor = 5.0f;          // Smoothness for smooth aim (1.0 = instant, 10.0 = very smooth)
        float thirdPersonDistance = 150.0f;    // Third person camera distance
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

            // Core Features
            file << "consoleDebugEnabled=" << settings.consoleDebugEnabled << "\n";
            file << "aimbotEnabled=" << settings.aimbotEnabled << "\n";
            file << "triggerbotEnabled=" << settings.triggerbotEnabled << "\n";
            file << "bhopEnabled=" << settings.bhopEnabled << "\n";
            file << "headAngleLineEnabled=" << settings.headAngleLineEnabled << "\n";
            file << "snaplinesEnabled=" << settings.snaplinesEnabled << "\n";
            file << "distanceESPEnabled=" << settings.distanceESPEnabled << "\n";
            file << "snaplinesWallCheckEnabled=" << settings.snaplinesWallCheckEnabled << "\n";
            file << "teamCheckEnabled=" << settings.teamCheckEnabled << "\n";
            file << "chamsEnabled=" << settings.chamsEnabled << "\n";
            
            // Extended Features
            file << "boneESPEnabled=" << settings.boneESPEnabled << "\n";
            file << "silentAimEnabled=" << settings.silentAimEnabled << "\n";
            file << "thirdPersonEnabled=" << settings.thirdPersonEnabled << "\n";
            file << "recoilCompEnabled=" << settings.recoilCompEnabled << "\n";
            file << "smoothAimEnabled=" << settings.smoothAimEnabled << "\n";
            file << "entityPredictorEnabled=" << settings.entityPredictorEnabled << "\n";
            file << "visibilityCheckEnabled=" << settings.visibilityCheckEnabled << "\n";
            
            // Parameters
            file << "smoothAimFactor=" << settings.smoothAimFactor << "\n";
            file << "thirdPersonDistance=" << settings.thirdPersonDistance << "\n";

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

                // Core Features
                if (key == "consoleDebugEnabled") settings.consoleDebugEnabled = boolValue;
                else if (key == "aimbotEnabled") settings.aimbotEnabled = boolValue;
                else if (key == "triggerbotEnabled") settings.triggerbotEnabled = boolValue;
                else if (key == "bhopEnabled") settings.bhopEnabled = boolValue;
                else if (key == "headAngleLineEnabled") settings.headAngleLineEnabled = boolValue;
                else if (key == "snaplinesEnabled") settings.snaplinesEnabled = boolValue;
                else if (key == "distanceESPEnabled") settings.distanceESPEnabled = boolValue;
                else if (key == "snaplinesWallCheckEnabled") settings.snaplinesWallCheckEnabled = boolValue;
                else if (key == "teamCheckEnabled") settings.teamCheckEnabled = boolValue;
                else if (key == "chamsEnabled") settings.chamsEnabled = boolValue;
                
                // Extended Features
                else if (key == "boneESPEnabled") settings.boneESPEnabled = boolValue;
                else if (key == "silentAimEnabled") settings.silentAimEnabled = boolValue;
                else if (key == "thirdPersonEnabled") settings.thirdPersonEnabled = boolValue;
                else if (key == "recoilCompEnabled") settings.recoilCompEnabled = boolValue;
                else if (key == "smoothAimEnabled") settings.smoothAimEnabled = boolValue;
                else if (key == "entityPredictorEnabled") settings.entityPredictorEnabled = boolValue;
                else if (key == "visibilityCheckEnabled") settings.visibilityCheckEnabled = boolValue;
                
                // Parameters
                else if (key == "smoothAimFactor") settings.smoothAimFactor = std::stof(value);
                else if (key == "thirdPersonDistance") settings.thirdPersonDistance = std::stof(value);
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
