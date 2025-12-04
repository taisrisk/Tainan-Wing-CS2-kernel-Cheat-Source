#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offset_manager.hpp"
#include "entity.hpp"
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class HeadAngleLine {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    std::uintptr_t localPlayerPawn;
    bool enabled;
    bool teamCheckEnabled;
    
    // Screen dimensions (default, will be updated)
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;
    
    // View matrix for world-to-screen
    float viewMatrix[16] = {0};
    
    // FOV for target filtering (same as aimbot)
    static constexpr float FOV_RADIUS = 250.0f;
    
    // HEAD OFFSETS - MUST MATCH AIMBOT VALUES
    static constexpr float HEAD_OFFSET_STANDING = 60.0f;
    static constexpr float HEAD_OFFSET_CROUCHED = 30.0f;
    
    // Screen position structure
    struct ScreenPos {
        float x, y;
        bool valid;
    };

public:
    HeadAngleLine(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), localPlayerPawn(0), enabled(false), teamCheckEnabled(true) {
        // Initialize view matrix to zeros
        for (int i = 0; i < 16; i++) {
            viewMatrix[i] = 0.0f;
        }
    }

    void setEnabled(bool enable) {
        enabled = enable;
    }

    bool isEnabled() const {
        return enabled;
    }
    
    void setTeamCheckEnabled(bool enable) {
        teamCheckEnabled = enable;
    }
    
    bool isTeamCheckEnabled() const {
        return teamCheckEnabled;
    }

    void setLocalPlayerPawn(std::uintptr_t pawn) {
        localPlayerPawn = pawn;
    }

    void setScreenDimensions(float width, float height) {
        screenWidth = width;
        screenHeight = height;
    }
    
    void updateViewMatrix(const float matrix[16]) {
        for (int i = 0; i < 16; i++) {
            viewMatrix[i] = matrix[i];
        }
    }

    // Calculate the Y position of the head angle line based on closest enemy's head
    struct LinePosition {
        float x;
        float y;
        bool valid;
    };

    LinePosition calculateLinePosition(const EntityManager& entities) {
        LinePosition result = { 0.0f, 0.0f, false };
        
        if (!enabled || localPlayerPawn == 0) {
            return result;
        }

        const auto& offsets = OffsetsManager::Get();
        
        // Get local player position
        std::uintptr_t localSceneNode = drv.read<std::uintptr_t>(localPlayerPawn + offsets.m_pGameSceneNode);
        if (localSceneNode == 0) {
            return result;
        }
        
        Vector3 localPos;
        drv.read_memory(reinterpret_cast<void*>(localSceneNode + offsets.m_vecAbsOrigin), &localPos, sizeof(Vector3));
        
        // Add head offset to local position (USING AIMBOT VALUES)
        localPos.z += HEAD_OFFSET_STANDING;
        
        // Find closest target to FOV center (respects team check setting)
        const Entity* closestTarget = findClosestTargetToFOV(entities, localPos);
        
        if (closestTarget == nullptr) {
            return result; // No valid target
        }
        
        // Get target head position (USING AIMBOT VALUES)
        Vector3 targetHeadPos = closestTarget->position;
        targetHeadPos.z += HEAD_OFFSET_STANDING; // Use standing offset (could add crouch detection later)
        
        // Project target head to screen
        ScreenPos targetScreen = worldToScreen(targetHeadPos);
        if (!targetScreen.valid) {
            return result;
        }
        
        // Line position is at the target's head Y position, centered X
        result.x = screenWidth * 0.5f;
        result.y = targetScreen.y;
        result.valid = true;
        
        return result;
    }

private:
    ScreenPos worldToScreen(const Vector3& world) {
        ScreenPos result = { 0.0f, 0.0f, false };
        
        float w = viewMatrix[12] * world.x + viewMatrix[13] * world.y +
                  viewMatrix[14] * world.z + viewMatrix[15];

        if (w < 0.01f) {
            return result;
        }

        float x = viewMatrix[0] * world.x + viewMatrix[1] * world.y +
                  viewMatrix[2] * world.z + viewMatrix[3];
        float y = viewMatrix[4] * world.x + viewMatrix[5] * world.y +
                  viewMatrix[6] * world.z + viewMatrix[7];

        x /= w;
        y /= w;

        result.x = (screenWidth / 2.0f) * (1.0f + x);
        result.y = (screenHeight / 2.0f) * (1.0f - y);
        result.valid = (result.x >= 0 && result.x <= screenWidth &&
                       result.y >= 0 && result.y <= screenHeight);

        return result;
    }
    
    const Entity* findClosestTargetToFOV(const EntityManager& entities, const Vector3& localPos) {
        const auto& offsets = OffsetsManager::Get();
        uint8_t localTeam = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);
        
        float screenCenterX = screenWidth / 2.0f;
        float screenCenterY = screenHeight / 2.0f;
        
        float bestDistance = (std::numeric_limits<float>::max)();
        const Entity* closestTarget = nullptr;
        
        // Check enemies first
        for (const auto& enemy : entities.enemy_entities) {
            if (!enemy.is_valid || enemy.health <= 0 || enemy.address == localPlayerPawn) {
                continue;
            }
            
            // Get enemy head position (USING AIMBOT VALUES)
            Vector3 enemyHeadPos = enemy.position;
            enemyHeadPos.z += HEAD_OFFSET_STANDING;
            
            // Project to screen
            ScreenPos screenPos = worldToScreen(enemyHeadPos);
            if (!screenPos.valid) {
                continue;
            }
            
            // Calculate distance from screen center
            float dx = screenPos.x - screenCenterX;
            float dy = screenPos.y - screenCenterY;
            float distanceFromCenter = std::sqrt(dx * dx + dy * dy);
            
            // Check if within FOV
            if (distanceFromCenter > FOV_RADIUS) {
                continue;
            }
            
            // Track closest to center
            if (distanceFromCenter < bestDistance) {
                bestDistance = distanceFromCenter;
                closestTarget = &enemy;
            }
        }
        
        // If team check is disabled, also check teammates
        if (!teamCheckEnabled) {
            for (const auto& teammate : entities.teammate_entities) {
                if (!teammate.is_valid || teammate.health <= 0 || teammate.address == localPlayerPawn) {
                    continue;
                }
                
                // Get teammate head position (USING AIMBOT VALUES)
                Vector3 teammateHeadPos = teammate.position;
                teammateHeadPos.z += HEAD_OFFSET_STANDING;
                
                // Project to screen
                ScreenPos screenPos = worldToScreen(teammateHeadPos);
                if (!screenPos.valid) {
                    continue;
                }
                
                // Calculate distance from screen center
                float dx = screenPos.x - screenCenterX;
                float dy = screenPos.y - screenCenterY;
                float distanceFromCenter = std::sqrt(dx * dx + dy * dy);
                
                // Check if within FOV
                if (distanceFromCenter > FOV_RADIUS) {
                    continue;
                }
                
                // Track closest to center
                if (distanceFromCenter < bestDistance) {
                    bestDistance = distanceFromCenter;
                    closestTarget = &teammate;
                }
            }
        }
        
        return closestTarget;
    }
};
