#pragma once

#include "../Features/triggerbot.hpp"
#include "../Features/bhop.hpp"
#include "../Features/Aimbot.hpp"
#include "../Features/Chams.hpp"
#include "../Features/BoneESP.hpp"
#include "../Features/ThirdPerson.hpp"
#include "../Features/VisibilityChecker.hpp"
#include "../Features/SmoothAim.hpp"
#include "../Features/RecoilCompensation.hpp"
#include "../Features/ImGuiESP.hpp"
#include "../Features/SilentAim.hpp"
#include "console_logger.hpp"
#include "settings_helper.hpp"
#include <cctype>

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
        SilentAim& silentAim,
        EntityPredictor& entityPredictor,
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
          recoilComp(recoilComp), espRenderer(espRenderer), silentAim(silentAim), entityPredictor(entityPredictor),
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
        aimbot.setPredictionEnabled(entityPredictorEnabled);
        aimbot.setSoftClampEnabled(smoothAimEnabled);
        chams.setEnabled(chamsEnabled);
        ConsoleLogger::setEnabled(consoleDebugEnabled);

        // team checks
        triggerbot.setTeamCheckEnabled(teamCheckEnabled);
        aimbot.setTeamCheckEnabled(teamCheckEnabled);
        espRenderer.setHeadAngleLineTeamCheckEnabled(teamCheckEnabled);
        espRenderer.setTeamCheckEnabled(teamCheckEnabled);

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
        // normalize to lowercase for letter keys
        int k = key;
        if (k >= 'A' && k <= 'Z') {
            k = std::tolower(k);
        }
        switch (k) {
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
                espRenderer.setTeamCheckEnabled(teamCheckEnabled);
                changed = true;
                break;
            case '9':
                chamsEnabled = !chamsEnabled;
                chams.setEnabled(chamsEnabled);
                espRenderer.setChamsEnabled(chamsEnabled);
                changed = true;
                break;
            case 'q': // Bone ESP
                boneESPEnabled = !boneESPEnabled;
                boneESP.setEnabled(boneESPEnabled);
                espRenderer.setBoneESPEnabled(boneESPEnabled);
                changed = true;
                break;
            case 'e': // Third Person
                thirdPersonEnabled = !thirdPersonEnabled;
                thirdPerson.setEnabled(thirdPersonEnabled);
                changed = true;
                break;
            case 'w': // Silent Aim
                silentAimEnabled = !silentAimEnabled;
                silentAim.setEnabled(silentAimEnabled);
                changed = true;
                break;
            case 'r': // Recoil Comp
                recoilCompEnabled = !recoilCompEnabled;
                changed = true;
                break;
            case 't': // Smooth Aim
                smoothAimEnabled = !smoothAimEnabled;
                changed = true;
                break;
            case 'y': // Prediction
                entityPredictorEnabled = !entityPredictorEnabled;
                aimbot.setPredictionEnabled(entityPredictorEnabled);
                changed = true;
                break;
            case 'u': // Visibility
                visibilityCheckEnabled = !visibilityCheckEnabled;
                visibilityChecker.setEnabled(visibilityCheckEnabled);
                changed = true;
                break;
            default:
                break;
        }

        if (changed) {
            SettingsManager::SaveSettings(SettingsHelper::BuildFromToggles(
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
            ));
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
    SilentAim& silentAim;
    EntityPredictor& entityPredictor;

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
