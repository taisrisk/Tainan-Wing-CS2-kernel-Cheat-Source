#include "features_init.hpp"
#include "console_colors.hpp"

namespace FeaturesInit {

Toggles LoadToggles() {
    SettingsManager::Settings s = SettingsManager::LoadSettings();
    Toggles t{};
    t.triggerbotEnabled = s.triggerbotEnabled;
    t.bhopEnabled = s.bhopEnabled;
    t.aimbotEnabled = s.aimbotEnabled;
    t.consoleDebugEnabled = s.consoleDebugEnabled;
    t.teamCheckEnabled = s.teamCheckEnabled;
    t.headAngleLineEnabled = s.headAngleLineEnabled;
    t.snaplinesEnabled = s.snaplinesEnabled;
    t.distanceESPEnabled = s.distanceESPEnabled;
    t.snaplinesWallCheckEnabled = s.snaplinesWallCheckEnabled;
    t.chamsEnabled = s.chamsEnabled;
    t.boneESPEnabled = s.boneESPEnabled;
    t.silentAimEnabled = s.silentAimEnabled;
    t.thirdPersonEnabled = s.thirdPersonEnabled;
    t.recoilCompEnabled = s.recoilCompEnabled;
    t.smoothAimEnabled = s.smoothAimEnabled;
    t.entityPredictorEnabled = s.entityPredictorEnabled;
    t.visibilityCheckEnabled = s.visibilityCheckEnabled;
    if (SettingsManager::SettingsFileExists()) {
        ConsoleColor::PrintSuccess("Settings loaded from file");
    } else {
        ConsoleColor::PrintInfo("No saved settings found - using defaults");
    }
    return t;
}

void ApplyToggles(Features& f, ImGuiESP& esp, const Toggles& t) {
    f.triggerbot.setEnabled(t.triggerbotEnabled);
    f.bhop.setEnabled(t.bhopEnabled);
    f.aimbot.setEnabled(t.aimbotEnabled);
    f.chams.setEnabled(t.chamsEnabled);
    ConsoleLogger::setEnabled(t.consoleDebugEnabled);
    f.triggerbot.setTeamCheckEnabled(t.teamCheckEnabled);
    f.aimbot.setTeamCheckEnabled(t.teamCheckEnabled);
    esp.setHeadAngleLineEnabled(t.headAngleLineEnabled);
    esp.setHeadAngleLineTeamCheckEnabled(t.teamCheckEnabled);
    esp.setSnaplinesEnabled(t.snaplinesEnabled);
    esp.setSnaplinesWallCheckEnabled(t.snaplinesWallCheckEnabled);
    esp.setDistanceESPEnabled(t.distanceESPEnabled);
    esp.setChamsEnabled(t.chamsEnabled);
    // Apply master overlay visibility
    esp.setMasterVisible(t.visibilityCheckEnabled);
}

}
