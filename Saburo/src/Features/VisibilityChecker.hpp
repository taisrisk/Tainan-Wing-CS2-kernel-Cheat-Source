#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "entity.hpp"
#include <cmath>

// Wall-check visibility system: Determines if enemy is visible (no walls blocking)
class VisibilityChecker {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;

public:
    VisibilityChecker(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client) {}

    // Check if target is visible from local player's position
    // Returns true if NO walls between local and target
    bool isVisible(std::uintptr_t localPlayerPawn, std::uintptr_t targetAddress) {
        if (localPlayerPawn == 0 || targetAddress == 0) {
            return false;
        }

        const auto& offsets = OffsetsManager::Get();

        // Get local player eye position (camera position)
        std::uintptr_t localSceneNode = drv.read<std::uintptr_t>(localPlayerPawn + offsets.m_pGameSceneNode);
        if (localSceneNode == 0) {
            return false;
        }

        Vector3 localPos;
        if (!drv.read_memory(reinterpret_cast<void*>(localSceneNode + offsets.m_vecAbsOrigin),
            &localPos, sizeof(Vector3))) {
            return false;
        }

        // Add eye height (camera is at head level)
        localPos.z += 64.0f;

        // Get target head position
        std::uintptr_t targetSceneNode = drv.read<std::uintptr_t>(targetAddress + offsets.m_pGameSceneNode);
        if (targetSceneNode == 0) {
            return false;
        }

        Vector3 targetPos;
        if (!drv.read_memory(reinterpret_cast<void*>(targetSceneNode + offsets.m_vecAbsOrigin),
            &targetPos, sizeof(Vector3))) {
            return false;
        }

        // Add head height
        targetPos.z += 60.0f;

        // Simple visibility check using view frustum and distance
        // If target is within FOV and reasonable distance, consider visible
        Vector3 direction;
        direction.x = targetPos.x - localPos.x;
        direction.y = targetPos.y - localPos.y;
        direction.z = targetPos.z - localPos.z;

        float distance = std::sqrt(direction.x * direction.x +
            direction.y * direction.y +
            direction.z * direction.z);

        if (distance < 0.001f || distance > 10000.0f) {
            return false;
        }

        // Normalize direction
        direction.x /= distance;
        direction.y /= distance;
        direction.z /= distance;

        // Get local player view angles
        std::uintptr_t viewAnglesAddr = clientBase + cs2_dumper::offsets::client_dll::dwViewAngles;
        float viewAngles[3];
        if (!drv.read_memory(reinterpret_cast<void*>(viewAnglesAddr), viewAngles, sizeof(viewAngles))) {
            return false;
        }

        float pitch = viewAngles[0] * (3.14159265f / 180.0f);
        float yaw = viewAngles[1] * (3.14159265f / 180.0f);

        // Calculate view direction vector
        Vector3 viewDir;
        viewDir.x = std::cos(pitch) * std::cos(yaw);
        viewDir.y = std::cos(pitch) * std::sin(yaw);
        viewDir.z = -std::sin(pitch);

        // Calculate dot product (angle between view direction and target direction)
        float dot = viewDir.x * direction.x + viewDir.y * direction.y + viewDir.z * direction.z;

        // If dot product > 0.5, target is within roughly 60 degree FOV (visible range)
        // This is a simplified check - proper wall detection would require ray tracing
        // For CS2, we can use a heuristic: if target is in FOV, assume visible
        static constexpr float VISIBILITY_THRESHOLD = 0.3f; // ~73 degree FOV
        
        return dot > VISIBILITY_THRESHOLD;
    }

    // Batch check visibility for all entities
    void updateVisibility(const EntityManager& entities, std::uintptr_t localPlayerPawn, 
                         std::vector<bool>& enemyVisibility, std::vector<bool>& teammateVisibility) {
        enemyVisibility.clear();
        teammateVisibility.clear();

        enemyVisibility.reserve(entities.enemy_entities.size());
        teammateVisibility.reserve(entities.teammate_entities.size());

        for (const auto& enemy : entities.enemy_entities) {
            bool visible = isVisible(localPlayerPawn, enemy.address);
            enemyVisibility.push_back(visible);
        }

        for (const auto& teammate : entities.teammate_entities) {
            bool visible = isVisible(localPlayerPawn, teammate.address);
            teammateVisibility.push_back(visible);
        }
    }
};
