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
        bool boneESPEnabled,
        bool silentAimEnabled,
        bool thirdPersonEnabled,
        bool recoilCompEnabled,
        bool smoothAimEnabled,
        bool entityPredictorEnabled,
        bool visibilityCheckEnabled
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
        s.boneESPEnabled = boneESPEnabled;
        s.silentAimEnabled = silentAimEnabled;
        s.thirdPersonEnabled = thirdPersonEnabled;
        s.recoilCompEnabled = recoilCompEnabled;
        s.smoothAimEnabled = smoothAimEnabled;
        s.entityPredictorEnabled = entityPredictorEnabled;
        s.visibilityCheckEnabled = visibilityCheckEnabled;
        return s;
    }
}
