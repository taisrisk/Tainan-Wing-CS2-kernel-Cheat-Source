#pragma once

#include "entity.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include "../Offsets/buttons.hpp"
#include "../Helpers/console_logger.hpp"
#include <chrono>
#include <unordered_set>
#include <windows.h>

class Triggerbot {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    std::uintptr_t localPlayerPawn;
    bool isEnabled;
    bool teamCheckEnabled;
    std::chrono::steady_clock::time_point lastShotTime;
    
    // Cache of valid enemy addresses for instant validation
    std::unordered_set<std::uintptr_t> validEnemyCache;
    
    // Cached local team for faster checks
    uint8_t cachedLocalTeam = 0;
    
    static constexpr int SHOT_COOLDOWN_MS = 200; // Reliable per-shot delay
    static constexpr int CACHE_REFRESH_MS = 100; // Refresh validation cache every 100ms

    // No recoil gating (simplified)

public:
    Triggerbot(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), localPlayerPawn(0),
        isEnabled(true), teamCheckEnabled(true), cachedLocalTeam(0) {
        // Initialize with time in the past so first shot can fire immediately
        lastShotTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(SHOT_COOLDOWN_MS);
    }

    void setEnabled(bool enabled) {
        isEnabled = enabled;
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("TB", enabled ? "ENABLED" : "DISABLED");
        }
    }

    bool getEnabled() const {
        return isEnabled;
    }

    void setTeamCheckEnabled(bool enabled) {
        teamCheckEnabled = enabled;
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("TB", enabled ? "Team check ENABLED" : "Team check DISABLED");
        }
    }

    bool isTeamCheckEnabled() const {
        return teamCheckEnabled;
    }

    void setLocalPlayerPawn(std::uintptr_t pawn) {
        localPlayerPawn = pawn;
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("TB", ("Local pawn set: 0x" + std::to_string(pawn)).c_str());
        }
    }

    // Update the enemy cache from ESP entities - call this before update()
    void updateEnemyCache(const EntityManager& entityManager) {
        validEnemyCache.clear();
        
        // Build cache of all valid alive enemies
        for (const auto& enemy : entityManager.enemy_entities) {
            if (enemy.is_valid && enemy.life_state == 0 && enemy.health > 0) {
                validEnemyCache.insert(enemy.address);
            }
        }
        
        // If team check disabled, also add teammates
        if (!teamCheckEnabled) {
            for (const auto& teammate : entityManager.teammate_entities) {
                if (teammate.is_valid && teammate.life_state == 0 && teammate.health > 0) {
                    validEnemyCache.insert(teammate.address);
                }
            }
        }
    }

    // INSTANT RESPONSIVE UPDATE - No delays, no validation waits
    void update() {
        if (!isEnabled || localPlayerPawn == 0) {
            return;
        }

        // Check shot cooldown FIRST - exit immediately if on cooldown
        auto now = std::chrono::steady_clock::now();
        long long timeSinceLastShot = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastShotTime).count();
        
        if (timeSinceLastShot < SHOT_COOLDOWN_MS) {
            return; // Still on cooldown, don't even check crosshair
        }

        // No recoil gating; rely on cooldown only

        // Simplified gating: rely on cooldown only for reliability

        // Get dynamic offsets
        const auto& offsets = OffsetsManager::Get();

        // Read crosshair entity handle - INSTANT
        uint32_t crosshairHandle = drv.read<uint32_t>(localPlayerPawn + offsets.m_iIDEntIndex);

        // Fast rejection - invalid handle
        if (crosshairHandle == 0 || crosshairHandle == 0xFFFF) {
            return;
        }

        // Fast entity resolution
        uint32_t chunk_index = (crosshairHandle >> 9) & 0x7F;
        uint32_t entityIndex = crosshairHandle & 0x1FF;

        // Get entity list
        std::uintptr_t entityList = drv.read<std::uintptr_t>(clientBase + OffsetsManager::Get().dwEntityList);
        if (entityList == 0) {
            return;
        }

        // Resolve entity address
        std::uintptr_t chunk_ptr_address = entityList + (0x8 * chunk_index) + 0x10;
        std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);
        if (chunk_ptr == 0 && chunk_index > 0) {
            // Fallback: try previous chunk index (some lists are 1-based)
            std::uintptr_t alt_addr = entityList + (0x8 * (chunk_index - 1)) + 0x10;
            chunk_ptr = drv.read<std::uintptr_t>(alt_addr);
        }
        if (chunk_ptr == 0) {
            return;
        }

        std::uintptr_t targetAddress = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entityIndex));
        if (targetAddress == 0) {
            return;
        }

        // Validation: when team-check is ON, use cache to prefer enemies; when OFF, bypass cache
        if (teamCheckEnabled) {
            if (validEnemyCache.find(targetAddress) == validEnemyCache.end()) {
                if (!quickValidateTarget(targetAddress, offsets)) {
                    return;
                }
            }
        } else {
            // Bypass cache; allow teammates if alive
            if (!quickValidateTarget(targetAddress, offsets)) {
                return;
            }
        }

        // TARGET IS VALID - SHOOT IMMEDIATELY
        shoot();
    }

private:
    // Quick inline validation for targets not in ESP cache
    bool quickValidateTarget(std::uintptr_t targetAddress, const CS2Offsets& offsets) {
        // Read health - must be alive
        int32_t health = drv.read<int32_t>(targetAddress + offsets.m_iHealth);
        if (health <= 0 || health > 100) {
            return false;
        }

        // Read life state - must be alive
        uint8_t lifeState = drv.read<uint8_t>(targetAddress + offsets.m_lifeState);
        if (lifeState != 0) {
            return false;
        }

        // Team check if enabled
        if (teamCheckEnabled) {
            uint8_t targetTeam = drv.read<uint8_t>(targetAddress + offsets.m_iTeamNum);
            uint8_t localTeam = drv.read<uint8_t>(localPlayerPawn + offsets.m_iTeamNum);
            
            if (targetTeam == localTeam) {
                return false; // Same team
            }
        }

        return true;
    }

    void shoot() {
        // Get attack button address
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t attackAddress = clientBase + offsets.buttons.attack;

        // TRY METHOD 1: Write to CS2 attack button (primary method)
        uint32_t pressValue = 65537; // 0x10001
        bool writeSuccess = drv.write_memory(reinterpret_cast<void*>(attackAddress), &pressValue, sizeof(pressValue));
        
        // CRITICAL: Tiny delay for game to register the press
        Sleep(1); // 1ms delay
        
        uint32_t releaseValue = 256; // 0x100
        drv.write_memory(reinterpret_cast<void*>(attackAddress), &releaseValue, sizeof(releaseValue));

        // METHOD 2: Mouse event fallback (always execute for reliability)
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(1); // 1ms delay
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

        // Update shot time for cooldown tracking
        lastShotTime = std::chrono::steady_clock::now();
        // No extra lockout
        
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("TB", writeSuccess ? "SHOT FIRED (BUTTON)" : "SHOT FIRED (MOUSE)");
        }
    }
};