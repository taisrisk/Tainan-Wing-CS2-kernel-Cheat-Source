#pragma once

#include "src/Features/triggerbot.hpp"
#include "src/Features/bhop.hpp"
#include "src/Features/Aimbot.hpp"
#include "src/Features/Chams.hpp"
#include "src/Features/BoneESP.hpp"
#include "src/Features/ThirdPerson.hpp"
#include "src/Features/VisibilityChecker.hpp"
#include "src/Features/SmoothAim.hpp"
#include "src/Features/RecoilCompensation.hpp"
#include "src/Features/ImGuiESP.hpp"
#include "src/Helpers/console_logger.hpp"
#include "src/Helpers/settings_helper.hpp"

class ToggleManager {
public:
    ToggleManager(
        // feature objects
        Triggerbot& triggerbot,
        BunnyHop& bhop,
        Aimbot& aimbot,
        Chams& chams,
        BoneESP& boneESP,
        ThirdPerson& thirdPerson,
        VisibilityChecker& visibilityChecker,
        SmoothAim& smoothAim,
        RecoilCompensation& recoilComp,
        ImGuiESP& espRenderer,
        // toggles (as references)
        bool& triggerbotEnabled,
        bool& bhopEnabled,
        bool& aimbotEnabled,
        bool& consoleDebugEnabled,
        bool& teamCheckEnabled,
        bool& headAngleLineEnabled,
        bool& snaplinesEnabled,
        bool& distanceESPEnabled,
        bool& snaplinesWallCheckEnabled,
        bool& chamsEnabled,
        bool& boneESPEnabled,
        bool& silentAimEnabled,
        bool& thirdPersonEnabled,
        bool& recoilCompEnabled,
        bool& smoothAimEnabled,
        bool& entityPredictorEnabled,
        bool& visibilityCheckEnabled
    )
        : triggerbot(triggerbot), bhop(bhop), aimbot(aimbot), chams(chams), boneESP(boneESP),
          thirdPerson(thirdPerson), visibilityChecker(visibilityChecker), smoothAim(smoothAim),
          recoilComp(recoilComp), espRenderer(espRenderer),
          triggerbotEnabled(triggerbotEnabled), bhopEnabled(bhopEnabled), aimbotEnabled(aimbotEnabled),
          consoleDebugEnabled(consoleDebugEnabled), teamCheckEnabled(teamCheckEnabled),
          headAngleLineEnabled(headAngleLineEnabled), snaplinesEnabled(snaplinesEnabled),
          distanceESPEnabled(distanceESPEnabled), snaplinesWallCheckEnabled(snaplinesWallCheckEnabled),
          chamsEnabled(chamsEnabled), boneESPEnabled(boneESPEnabled), silentAimEnabled(silentAimEnabled),
          thirdPersonEnabled(thirdPersonEnabled), recoilCompEnabled(recoilCompEnabled), smoothAimEnabled(smoothAimEnabled),
          entityPredictorEnabled(entityPredictorEnabled), visibilityCheckEnabled(visibilityCheckEnabled) {}

    void applyAll() {
        // apply primary features
        triggerbot.setEnabled(triggerbotEnabled);
        bhop.setEnabled(bhopEnabled);
        aimbot.setEnabled(aimbotEnabled);
        chams.setEnabled(chamsEnabled);
        ConsoleLogger::setEnabled(consoleDebugEnabled);

        // team checks
        triggerbot.setTeamCheckEnabled(teamCheckEnabled);
        aimbot.setTeamCheckEnabled(teamCheckEnabled);
        espRenderer.setHeadAngleLineTeamCheckEnabled(teamCheckEnabled);

        // ESP related
        espRenderer.setHeadAngleLineEnabled(headAngleLineEnabled);
        espRenderer.setSnaplinesEnabled(snaplinesEnabled);
        espRenderer.setDistanceESPEnabled(distanceESPEnabled);
        espRenderer.setSnaplinesWallCheckEnabled(snaplinesWallCheckEnabled);
        espRenderer.setChamsEnabled(chamsEnabled);
        espRenderer.setBoneESPEnabled(boneESPEnabled);

        // extended features
        boneESP.setEnabled(boneESPEnabled);
        thirdPerson.setEnabled(thirdPersonEnabled);
        visibilityChecker.setEnabled(visibilityCheckEnabled);
    }

    void handleKey(int key) {
        bool changed = false;
        switch (key) {
            case '0':
                consoleDebugEnabled = !consoleDebugEnabled;
                ConsoleLogger::setEnabled(consoleDebugEnabled);
                changed = true;
                break;
            case '1':
                aimbotEnabled = !aimbotEnabled;
                aimbot.setEnabled(aimbotEnabled);
                changed = true;
                break;
            case '2':
                triggerbotEnabled = !triggerbotEnabled;
                triggerbot.setEnabled(triggerbotEnabled);
                changed = true;
                break;
            case '3':
                bhopEnabled = !bhopEnabled;
                bhop.setEnabled(bhopEnabled);
                changed = true;
                break;
            case '4':
                headAngleLineEnabled = !headAngleLineEnabled;
                espRenderer.setHeadAngleLineEnabled(headAngleLineEnabled);
                changed = true;
                break;
            case '5':
                snaplinesEnabled = !snaplinesEnabled;
                espRenderer.setSnaplinesEnabled(snaplinesEnabled);
                changed = true;
                break;
            case '6':
                distanceESPEnabled = !distanceESPEnabled;
                espRenderer.setDistanceESPEnabled(distanceESPEnabled);
                changed = true;
                break;
            case '7':
                snaplinesWallCheckEnabled = !snaplinesWallCheckEnabled;
                espRenderer.setSnaplinesWallCheckEnabled(snaplinesWallCheckEnabled);
                changed = true;
                break;
            case '8':
                teamCheckEnabled = !teamCheckEnabled;
                triggerbot.setTeamCheckEnabled(teamCheckEnabled);
                aimbot.setTeamCheckEnabled(teamCheckEnabled);
                espRenderer.setHeadAngleLineTeamCheckEnabled(teamCheckEnabled);
                changed = true;
                break;
            case '9':
                chamsEnabled = !chamsEnabled;
                chams.setEnabled(chamsEnabled);
                espRenderer.setChamsEnabled(chamsEnabled);
                changed = true;
                break;
            default:
                break;
        }

        if (changed) {
            SettingsHelper::SaveFromToggles(
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
        }
    }

private:
    // feature objects
    Triggerbot& triggerbot;
    BunnyHop& bhop;
    Aimbot& aimbot;
    Chams& chams;
    BoneESP& boneESP;
    ThirdPerson& thirdPerson;
    VisibilityChecker& visibilityChecker;
    SmoothAim& smoothAim;
    RecoilCompensation& recoilComp;
    ImGuiESP& espRenderer;

    // toggles
    bool& triggerbotEnabled;
    bool& bhopEnabled;
    bool& aimbotEnabled;
    bool& consoleDebugEnabled;
    bool& teamCheckEnabled;
    bool& headAngleLineEnabled;
    bool& snaplinesEnabled;
    bool& distanceESPEnabled;
    bool& snaplinesWallCheckEnabled;
    bool& chamsEnabled;
    bool& boneESPEnabled;
    bool& silentAimEnabled;
    bool& thirdPersonEnabled;
    bool& recoilCompEnabled;
    bool& smoothAimEnabled;
    bool& entityPredictorEnabled;
    bool& visibilityCheckEnabled;
};
