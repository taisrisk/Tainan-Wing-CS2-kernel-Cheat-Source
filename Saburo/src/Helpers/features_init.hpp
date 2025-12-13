#pragma once
#include <cstdint>
#include "../Core/driver.hpp"
#include "../Features/ImGuiESP.hpp"
#include "../Features/triggerbot.hpp"
#include "../Features/bhop.hpp"
#include "../Features/Aimbot.hpp"
#include "../Features/Chams.hpp"
#include "../Features/BoneESP.hpp"
#include "../Features/ThirdPerson.hpp"
#include "../Features/VisibilityChecker.hpp"
#include "../Features/SmoothAim.hpp"
#include "../Features/RecoilCompensation.hpp"
#include "../Features/SilentAim.hpp"
#include "../Features/EntityPredictor.hpp"
#include "../Helpers/settings_manager.hpp"

struct Features {
    Triggerbot triggerbot;
    BunnyHop bhop;
    Aimbot aimbot;
    Chams chams;
    BoneESP boneEsp;
    ThirdPerson thirdPerson;
    VisibilityChecker visibilityChecker;
    SmoothAim smoothAim;
    RecoilCompensation recoilComp;
    SilentAim silentAim;
    EntityPredictor entityPredictor;
    Features(driver::DriverHandle& drv, std::uintptr_t client)
        : triggerbot(drv, client), bhop(drv, client), aimbot(drv, client), chams(drv, client),
          boneEsp(drv, client), thirdPerson(drv, client), visibilityChecker(drv, client),
          smoothAim(), recoilComp(drv, client), silentAim(drv, client), entityPredictor(drv) {}
};

struct Toggles {
    bool triggerbotEnabled;
    bool bhopEnabled;
    bool aimbotEnabled;
    bool consoleDebugEnabled;
    bool teamCheckEnabled;
    bool headAngleLineEnabled;
    bool snaplinesEnabled;
    bool distanceESPEnabled;
    bool snaplinesWallCheckEnabled;
    bool chamsEnabled;
    bool boneESPEnabled;
    bool silentAimEnabled;
    bool thirdPersonEnabled;
    bool recoilCompEnabled;
    bool smoothAimEnabled;
    bool entityPredictorEnabled;
    bool visibilityCheckEnabled;
};

namespace FeaturesInit {
    Toggles LoadToggles();
    void ApplyToggles(Features& f, ImGuiESP& esp, const Toggles& t);
}
