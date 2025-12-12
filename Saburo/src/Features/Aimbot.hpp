#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "entity.hpp"
#include "EntityPredictor.hpp"
#include <cmath>
#include <limits>
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Aimbot {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    std::uintptr_t localPlayerPawn = 0;
    EntityPredictor predictor;
    bool predictionEnabled = false;
    bool softClampEnabled = false;

    // FIXED: Separate offsets for standing and crouching
    static constexpr float HEAD_OFFSET_STANDING = 60.0f;
    static constexpr float HEAD_OFFSET_CROUCHED = 30.0f;
    static constexpr float FOV_RADIUS = 600.0f;

    float smoothing = 0.0f;
    bool enabled = false;

    float viewMatrix[16] = { 0 };
    float screenWidth;
    float screenHeight;

    // Prediction params
    float baseLeadMs = 5.0f;           // small anticipatory lead
    float bulletSpeedUPS = 6000.0f;    // higher speed -> smaller lead

    // Persistent target locking
    std::uintptr_t lockedTargetAddress = 0;
    bool targetLocked = false;

    // Validation for map/round changes
    std::uintptr_t lastValidatedPlayerPawn = 0;
    int consecutiveInvalidReads = 0;
    static constexpr int MAX_INVALID_READS = 10;

    // Track last known player address for map change detection
    std::uintptr_t lastKnownLocalPlayerAddress = 0;
    
    // NEW: Track game state to detect deaths/round changes
    int32_t lastKnownHealth = 100;
    uint8_t lastKnownTeam = 0;
    int framesSinceDeath = 0;
    static constexpr int DEATH_RESET_FRAMES = 30;  // Reset after 30 frames of being dead

    struct ViewAngles {
        float pitch;
        float yaw;
        float roll;
        ViewAngles() : pitch(0), yaw(0), roll(0) {}
        ViewAngles(float p, float y, float r) : pitch(p), yaw(y), roll(r) {}
    };

    ViewAngles currentAngles;

    // Private nested struct
    struct ScreenPosition {
        float x;
        float y;
        bool valid;
    };

public:
    Aimbot(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), predictor(driver) {
        screenWidth = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
        screenHeight = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    }

    void setPredictionEnabled(bool enable) { predictionEnabled = enable; }
    void setSoftClampEnabled(bool enable) { softClampEnabled = enable; }

    void setLocalPlayerPawn(std::uintptr_t pawn) {
        // Only reset state if the address actually changed
        bool addressChanged = (pawn != 0 && pawn != localPlayerPawn);
        
        // Always update the address
        localPlayerPawn = pawn;
        lastValidatedPlayerPawn = pawn;
        lastKnownLocalPlayerAddress = pawn;
        
        // Reset lock when player address changes (respawn, map change, etc.)
        if (addressChanged) {
            // CRITICAL: Always clear lock and reset state on address change
            lockedTargetAddress = 0;
            targetLocked = false;
            consecutiveInvalidReads = 0;
            framesSinceDeath = 0;
            lastKnownHealth = 100;
            lastKnownTeam = 0;
        }
        else {
            // Even if address didn't change, reset invalid read counter
            consecutiveInvalidReads = 0;
        }
    }

    void setEnabled(bool enable) {
        enabled = enable;
        if (!enable) {
            lockedTargetAddress = 0;
            targetLocked = false;
            framesSinceDeath = 0;
        }
    }

    bool isEnabled() const {
        return enabled;
    }

    void setSmoothing(float value) {
        smoothing = value;
    }

    void update(const EntityManager& entities);

    void setTeamCheckEnabled(bool enable) { teamCheckEnabled = enable; }
    bool isTeamCheckEnabled() const { return teamCheckEnabled; }

private:
    // IMPROVED: Better local player validation
    bool validateLocalPlayer();

    // NEW: Detect if target is crouched (uses m_bDucked offset)
    bool isTargetCrouched(std::uintptr_t targetAddress);

    // NEW: Get dynamic head offset based on crouch state
    float getHeadOffset(std::uintptr_t targetAddress);

    void updateViewMatrix();
    bool worldToScreen(const Vector3& world, ScreenPosition& screen);
    Vector3 getLocalPlayerPosition();
    const Entity* findClosestTarget(const EntityManager& entities);
    ScreenPosition calculateAimPosition(const Entity& target);
    void aimAtTarget(const Entity& target);
    void checkGameState();
    bool hasEntityStateChanged(const Entity& entity);
    bool setViewAngles(const ViewAngles& angles);
    ViewAngles getViewAngles();
    ViewAngles calculateAimAngles(const Vector3& targetWorldPos);

    ScreenPosition getScreenCenter() const {
        ScreenPosition center;
        center.x = screenWidth / 2.0f;
        center.y = screenHeight / 2.0f;
        center.valid = true;
        return center;
    }

    float getScreenDistance(const ScreenPosition& a, const ScreenPosition& b) const {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    
    float get3DDistance(const Vector3& a, const Vector3& b) const {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    bool isRightClickHeld() const {
        return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    }

    bool teamCheckEnabled;  // NEW: Allow aiming at teammates when false
};