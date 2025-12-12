#pragma once

#include <vector>
#include <string>
#include "src/Helpers/console_colors.hpp"

namespace UIHelper {
    inline void PrintStartupHeader() {
        ConsoleColor::PrintHeader("SUBARO INITIALIZING");
        ConsoleColor::PrintLogo();
    }

    inline void PrintReattachNotice() {
        std::vector<std::string> boxLines = {
            "",
            "No players detected for 5 seconds.",
            "Returning to lobby detection...",
            ""
        };
        ConsoleColor::PrintBox("RE-ATTACH TRIGGERED", boxLines);
    }

    inline void PrintUnifiedPanel(
        int frame,
        int total,
        int enemies,
        int teammates,
        bool aimbotEnabled,
        bool triggerbotEnabled,
        bool bhopEnabled,
        bool headAngleLineEnabled,
        bool snaplinesEnabled,
        bool distanceESPEnabled,
        bool snaplinesWallCheckEnabled,
        bool teamCheckEnabled,
        bool chamsEnabled,
        bool boneESPEnabled,
        bool silentAimEnabled,
        bool thirdPersonEnabled,
        bool recoilCompEnabled,
        bool smoothAimEnabled,
        bool entityPredictorEnabled,
        bool visibilityCheckEnabled,
        bool consoleDebugEnabled,
        bool isInGame
    ) {
        ConsoleColor::PrintUnifiedPanel(
            frame,
            total,
            enemies,
            teammates,
            aimbotEnabled,
            triggerbotEnabled,
            bhopEnabled,
            headAngleLineEnabled,
            snaplinesEnabled,
            distanceESPEnabled,
            snaplinesWallCheckEnabled,
            teamCheckEnabled,
            chamsEnabled,
            boneESPEnabled,
            silentAimEnabled,
            thirdPersonEnabled,
            recoilCompEnabled,
            smoothAimEnabled,
            entityPredictorEnabled,
            visibilityCheckEnabled,
            consoleDebugEnabled,
            isInGame
        );
    }
}
