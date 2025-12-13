#pragma once
#include "settings_manager.hpp"

namespace SettingsHelper {
    inline SettingsManager::Settings BuildFromToggles(
        bool triggerbotEnabled,
        bool bhopEnabled,
        bool aimbotEnabled,
        bool consoleDebugEnabled,
        bool teamCheckEnabled,
        bool headAngleLineEnabled,
        bool snaplinesEnabled,
        bool distanceESPEnabled,
        bool snaplinesWallCheckEnabled,
        bool chamsEnabled,
        bool /*boneESPEnabled*/,        // ignored by current SettingsManager
        bool /*silentAimEnabled*/,      // ignored
        bool /*thirdPersonEnabled*/,    // ignored
        bool /*recoilCompEnabled*/,     // ignored
        bool /*smoothAimEnabled*/,      // ignored
        bool /*entityPredictorEnabled*/,// ignored
        bool /*visibilityCheckEnabled*/ // ignored
    ) {
        SettingsManager::Settings s;
        s.triggerbotEnabled = triggerbotEnabled;
        s.bhopEnabled = bhopEnabled;
        s.aimbotEnabled = aimbotEnabled;
        s.consoleDebugEnabled = consoleDebugEnabled;
        s.teamCheckEnabled = teamCheckEnabled;
        s.headAngleLineEnabled = headAngleLineEnabled;
        s.snaplinesEnabled = snaplinesEnabled;
        s.distanceESPEnabled = distanceESPEnabled;
        s.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
        s.chamsEnabled = chamsEnabled;
        return s;
    }
}
