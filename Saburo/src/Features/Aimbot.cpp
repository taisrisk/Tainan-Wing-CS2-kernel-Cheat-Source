#include "Aimbot.hpp"
#include "EntityPredictor.hpp"
#include "../Helpers/console_logger.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offset_manager.hpp"
#include <cmath>
#include <algorithm>

#undef max

// NEW: Cached entity data to avoid redundant reads
struct CachedEntityData {
    std::uintptr_t address = 0;
    Vector3 position = {0, 0, 0};
    int32_t health = 0;
    uint8_t team = 0;
    uint8_t lifeState = 0;
    bool crouched = false;
    bool valid = false;
};

void Aimbot::update(const EntityManager& entities) {
    if (!enabled) {
        return;
    }
    
    // Check if right mouse is held FIRST
    if (!isRightClickHeld()) {
        if (targetLocked) {
            lockedTargetAddress = 0;
            targetLocked = false;
        }
        return;
    }
    
    if (localPlayerPawn == 0) {
        return;
    }

    // Get offsets once
    const auto& offsets = OffsetsManager::Get();
    
    // Quick health check
    int32_t currentHealth = drv.read<int32_t>(localPlayerPawn + offsets.m_iHealth);
    if (currentHealth <= 0) {
        if (targetLocked) {
            lockedTargetAddress = 0;
            targetLocked = false;
        }
        return;
    }

    // Update view matrix every frame
    updateViewMatrix();

    // Update predictor history for alive entities (only if enabled)
    if (predictionEnabled) {
        for (const auto& e : entities.alive_entities) {
            if (!e.is_valid || e.health <= 0) continue;
            predictor.updateEntity(e.address, e.position);
        }
    }

    // Find best target
    const Entity* bestTarget = findClosestTarget(entities);

    // If no valid targets in FOV, clear lock
    if (bestTarget == nullptr) {
        if (targetLocked) {
            lockedTargetAddress = 0;
            targetLocked = false;
        }
        return;
    }

    // Update lock to best target
    lockedTargetAddress = bestTarget->address;
    targetLocked = true;

    // ===== AIM AT TARGET WITH PREDICTION =====
    Vector3 targetPos = bestTarget->position;
    targetPos.z += getHeadOffset(bestTarget->address);

    // Get local eye position
    Vector3 localPos = getLocalPlayerPosition();

    // Calculate initial direction to estimate travel time
    Vector3 direction;
    direction.x = targetPos.x - localPos.x;
    direction.y = targetPos.y - localPos.y;
    direction.z = targetPos.z - localPos.z;

    float distance = std::sqrt(direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);

    if (distance < 0.001f) return;

    // Predict target position based on velocity, net/input delay and bullet travel
    if (predictionEnabled) {
        float travelMs = (distance / std::max(1.0f, bulletSpeedUPS)) * 1000.0f;
        float leadMs = std::clamp(baseLeadMs + travelMs, 0.0f, 30.0f);
        Vector3 predicted = predictor.predictPosition(bestTarget->address, leadMs);
        if (!(predicted.x == 0.0f && predicted.y == 0.0f && predicted.z == 0.0f)) {
            targetPos = predicted;
            targetPos.z += getHeadOffset(bestTarget->address);

            // Recompute direction and distance
            direction.x = targetPos.x - localPos.x;
            direction.y = targetPos.y - localPos.y;
            direction.z = targetPos.z - localPos.z;
            distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (distance < 0.001f) return;
        }
    }

    // Calculate angles (clamp asin input to avoid NaN)
    float zRatio = direction.z / distance;
    if (zRatio > 1.0f) zRatio = 1.0f; else if (zRatio < -1.0f) zRatio = -1.0f;
    float targetPitch = -std::asin(zRatio) * (180.0f / M_PI);
    float targetYaw = std::atan2(direction.y, direction.x) * (180.0f / M_PI);

    // No recoil compensation applied here

    // Normalize
    while (targetYaw > 180.0f) targetYaw -= 360.0f;
    while (targetYaw < -180.0f) targetYaw += 360.0f;
    targetPitch = std::clamp(targetPitch, -89.0f, 89.0f);

    // ===== WRITE ANGLES =====
    std::uintptr_t viewAnglesAddr = clientBase + cs2_dumper::offsets::client_dll::dwViewAngles;
    if (softClampEnabled) {
        float currentAngles[3] = {0};
        drv.read_memory(reinterpret_cast<void*>(viewAnglesAddr), currentAngles, sizeof(currentAngles));
        float currPitch = currentAngles[0];
        float currYaw = currentAngles[1];

        float maxDeltaPitch = 4.0f; // deg per frame
        float maxDeltaYaw   = 6.0f;
        auto clampAbs = [](float v, float m){ return (v > m) ? m : (v < -m ? -m : v); };

        float dyaw = targetYaw - currYaw;
        while (dyaw > 180.0f) dyaw -= 360.0f;
        while (dyaw < -180.0f) dyaw += 360.0f;
        float dpitch = targetPitch - currPitch;

        dyaw = clampAbs(dyaw, maxDeltaYaw);
        dpitch = clampAbs(dpitch, maxDeltaPitch);

        float outPitch = currPitch + dpitch;
        float outYaw   = currYaw + dyaw;
        if (outPitch > 89.0f) outPitch = 89.0f; else if (outPitch < -89.0f) outPitch = -89.0f;

        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr), &outPitch, sizeof(float));
        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr + sizeof(float)), &outYaw, sizeof(float));
        float roll = 0.0f;
        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr + 2 * sizeof(float)), &roll, sizeof(float));
    } else {
        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr), &targetPitch, sizeof(float));
        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr + sizeof(float)), &targetYaw, sizeof(float));
        float roll = 0.0f;
        drv.write_memory(reinterpret_cast<void*>(viewAnglesAddr + 2 * sizeof(float)), &roll, sizeof(float));
    }
}

bool Aimbot::validateLocalPlayer() {
    if (localPlayerPawn == 0) {
        return false;
    }

    const auto& offsets = OffsetsManager::Get();

    // Try to read local player's health to validate the address is still valid
    int32_t health = drv.read<int32_t>(localPlayerPawn + offsets.m_iHealth);
    uint8_t team = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);
    uint8_t lifeState = drv.read<uint8_t>(localPlayerPawn + offsets.m_lifeState);

    // If health is invalid, team is 0, or lifestate shows dead, address is bad or player is dead
    if (health <= 0 || health > 500 || team == 0 || lifeState != 0) {
        consecutiveInvalidReads++;
        
        // Immediately clear lock if we detect we're dead or invalid
        if (targetLocked) {
            lockedTargetAddress = 0;
            targetLocked = false;
        }
        
        if (consecutiveInvalidReads >= MAX_INVALID_READS) {
            ConsoleLogger::logInfo("AIMBOT", "Local player address invalid - waiting for re-initialization");
            localPlayerPawn = 0;
            return false;
        }
        
        // Allow a few invalid reads (during death/respawn transition)
        return false;
    }
    else {
        // Valid read - reset counter
        consecutiveInvalidReads = 0;
    }

    return true;
}

// ===== NEW: CROUCH DETECTION =====
bool Aimbot::isTargetCrouched(std::uintptr_t targetAddress) {
    if (targetAddress == 0) {
        return false;
    }

    const auto& offsets = OffsetsManager::Get();
    
    try {
        // Get movement services pointer from player pawn
        std::uintptr_t movementServices = drv.read<std::uintptr_t>(targetAddress + offsets.m_pMovementServices);
        if (movementServices == 0) {
            return false;
        }
        
        // Read m_bInCrouch from movement services (more reliable than m_bDucked)
        // This is a bool at offset 0x240 from the movement services pointer
        bool inCrouch = drv.read<bool>(movementServices + offsets.m_bInCrouch);
        return inCrouch;
    }
    catch (...) {
        return false;
    }
}

// ===== NEW: DYNAMIC HEAD OFFSET =====
float Aimbot::getHeadOffset(std::uintptr_t targetAddress) {
    return isTargetCrouched(targetAddress) ? HEAD_OFFSET_CROUCHED : HEAD_OFFSET_STANDING;
}

void Aimbot::updateViewMatrix() {
    std::uintptr_t matrixAddress = clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix;
    drv.read_memory(reinterpret_cast<void*>(matrixAddress), viewMatrix, sizeof(viewMatrix));
}

bool Aimbot::worldToScreen(const Vector3& world, ScreenPosition& screen) {
    float w = viewMatrix[12] * world.x + viewMatrix[13] * world.y +
        viewMatrix[14] * world.z + viewMatrix[15];

    if (w < 0.01f) {
        screen.valid = false;
        return false;
    }

    float x = viewMatrix[0] * world.x + viewMatrix[1] * world.y +
        viewMatrix[2] * world.z + viewMatrix[3];
    float y = viewMatrix[4] * world.x + viewMatrix[5] * world.y +
        viewMatrix[6] * world.z + viewMatrix[7];

    x /= w;
    y /= w;

    screen.x = (screenWidth / 2.0f) * (1.0f + x);
    screen.y = (screenHeight / 2.0f) * (1.0f - y);
    screen.valid = (screen.x >= 0 && screen.x <= screenWidth &&
        screen.y >= 0 && screen.y <= screenHeight);

    return screen.valid;
}

Vector3 Aimbot::getLocalPlayerPosition() {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    
    if (localPlayerPawn == 0) {
        return position;
    }

    const auto& offsets = OffsetsManager::Get();
    std::uintptr_t gameSceneNode = drv.read<std::uintptr_t>(localPlayerPawn + offsets.m_pGameSceneNode);
    
    if (gameSceneNode == 0) {
        return position;
    }

    drv.read_memory(reinterpret_cast<void*>(gameSceneNode + offsets.m_vecAbsOrigin),
        &position, sizeof(Vector3));
    
    // Validate position before adding head offset
    if (std::isnan(position.x) || std::isnan(position.y) || std::isnan(position.z) ||
        std::abs(position.x) > 100000.0f || std::abs(position.y) > 100000.0f || std::abs(position.z) > 100000.0f) {
        // Return zero position if invalid
        return Vector3{ 0.0f, 0.0f, 0.0f };
    }
    
    position.z += getHeadOffset(localPlayerPawn);

    return position;
}

// ===== OPTIMIZED: FIND CLOSEST TARGET - REDUCED MEMORY READS =====
const Entity* Aimbot::findClosestTarget(const EntityManager& entities) {
    ScreenPosition screenCenter = getScreenCenter();
    Vector3 localPos = getLocalPlayerPosition();
    
    float bestScore = std::numeric_limits<float>::max();
    const Entity* closestEnemy = nullptr;
    
    static constexpr float MAX_LOCK_DISTANCE = 5000.0f;

    const auto& offsets = OffsetsManager::Get();
    uint8_t localTeamNow = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);

    const auto& targetList = teamCheckEnabled ? entities.enemy_entities : entities.alive_entities;

    for (const auto& enemy : targetList) {
        // Skip invalid or self
        if (!enemy.is_valid || enemy.address == localPlayerPawn) continue;
        
        // OPTIMIZED: Use cached entity data from EntityManager instead of re-reading
        // Entity struct already has health, team, position - don't re-read!
        if (enemy.health <= 0) continue;
        
        // Only check team if team check is enabled (use cached data)
        if (teamCheckEnabled && (enemy.team == localTeamNow || enemy.team == 0)) continue;

        // Use cached position from entity (already read by EntityEnumerator)
        Vector3 currentPos = enemy.position;
        currentPos.z += getHeadOffset(enemy.address);

        // Validate cached position
        if (std::isnan(currentPos.x) || std::isnan(currentPos.y) || std::isnan(currentPos.z) ||
            std::abs(currentPos.x) > 100000.0f || std::abs(currentPos.y) > 100000.0f || std::abs(currentPos.z) > 100000.0f) {
            continue;
        }
        
        // Calculate 3D distance
        float distance3D = get3DDistance(localPos, currentPos);
        
        if (distance3D > MAX_LOCK_DISTANCE) continue;

        // Project to screen
        ScreenPosition screenPos;
        if (!worldToScreen(currentPos, screenPos)) continue;

        // Distance from center
        float dx = screenPos.x - screenCenter.x;
        float dy = screenPos.y - screenCenter.y;
        float distanceFromCenter = std::sqrt(dx * dx + dy * dy);

        if (distanceFromCenter > FOV_RADIUS) continue;

        // Combined score: 70% screen distance, 30% 3D distance
        float normalizedDistance3D = distance3D / 10.0f;
        float combinedScore = (0.7f * distanceFromCenter) + (0.3f * normalizedDistance3D);

        if (combinedScore < bestScore) {
            bestScore = combinedScore;
            closestEnemy = &enemy;
        }
    }

    return closestEnemy;
}

Aimbot::ScreenPosition Aimbot::calculateAimPosition(const Entity& target) {
    Vector3 headPos = target.position;
    headPos.z += getHeadOffset(target.address);
    ScreenPosition screenPos;
    worldToScreen(headPos, screenPos);
    return screenPos;
}

void Aimbot::checkGameState() {
    // Not used in this simplified version
}

bool Aimbot::hasEntityStateChanged(const Entity& entity) {
    // Not used in this simplified version
    return false;
}

bool Aimbot::setViewAngles(const ViewAngles& angles) {
    return true;
}

Aimbot::ViewAngles Aimbot::getViewAngles() {
    ViewAngles angles;
    return angles;
}

Aimbot::ViewAngles Aimbot::calculateAimAngles(const Vector3& targetWorldPos) {
    ViewAngles angles;
    return angles;
}

void Aimbot::aimAtTarget(const Entity& target) {
    // Not used in this simplified version
}