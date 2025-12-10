#pragma once

#include "src/Helpers/settings_manager.hpp"

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
        SettingsManager::Settings settings{};
        settings.triggerbotEnabled = triggerbotEnabled;
        settings.bhopEnabled = bhopEnabled;
        settings.aimbotEnabled = aimbotEnabled;
        settings.consoleDebugEnabled = consoleDebugEnabled;
        settings.teamCheckEnabled = teamCheckEnabled;
        settings.headAngleLineEnabled = headAngleLineEnabled;
        settings.snaplinesEnabled = snaplinesEnabled;
        settings.distanceESPEnabled = distanceESPEnabled;
        settings.snaplinesWallCheckEnabled = snaplinesWallCheckEnabled;
        settings.chamsEnabled = chamsEnabled;
        settings.boneESPEnabled = boneESPEnabled;
        settings.silentAimEnabled = silentAimEnabled;
        settings.thirdPersonEnabled = thirdPersonEnabled;
        settings.recoilCompEnabled = recoilCompEnabled;
        settings.smoothAimEnabled = smoothAimEnabled;
        settings.entityPredictorEnabled = entityPredictorEnabled;
        settings.visibilityCheckEnabled = visibilityCheckEnabled;
        return settings;
    }

    inline void SaveFromToggles(
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
        auto s = BuildFromToggles(
            triggerbotEnabled,
            bhopEnabled,
            aimbotEnabled,
            consoleDebugEnabled,
            teamCheckEnabled,
            headAngleLineEnabled,
            snaplinesEnabled,
            distanceESPEnabled,
            snaplinesWallCheckEnabled,
            chamsEnabled,
            boneESPEnabled,
            silentAimEnabled,
            thirdPersonEnabled,
            recoilCompEnabled,
            smoothAimEnabled,
            entityPredictorEnabled,
            visibilityCheckEnabled
        );
        SettingsManager::SaveSettings(s);
    }
}
