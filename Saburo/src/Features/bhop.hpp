#pragma once

#include "entity.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include "../Offsets/buttons.hpp"
#include "../Helpers/console_logger.hpp"
#include <chrono>
#include <thread>

// Player flags for CS2
namespace PlayerFlags {
    constexpr uint32_t FL_ONGROUND = (1 << 0);
}

class BunnyHop {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    std::uintptr_t localPlayerPawn;
    bool isEnabled;
    std::chrono::steady_clock::time_point lastJumpTime;
    int jumpCount;
    bool lastSpaceState;

    static constexpr int JUMP_VELOCITY = 280;
    static constexpr int JUMP_INTERVAL_MS = 50;

public:
    BunnyHop(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), localPlayerPawn(0), isEnabled(false), jumpCount(0), lastSpaceState(false) {
        lastJumpTime = std::chrono::steady_clock::now();
    }

    void setEnabled(bool enabled) {
        isEnabled = enabled;
        jumpCount = 0;
        lastSpaceState = false;  // Reset state when toggled
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("BH", enabled ? "ENABLED" : "DISABLED");
        }
    }

    bool getEnabled() const {
        return isEnabled;
    }

    void setLocalPlayerPawn(std::uintptr_t pawn) {
        localPlayerPawn = pawn;
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("BH", ("Local pawn set: 0x" + std::to_string(pawn)).c_str());
        }
    }

    // Main update - FIX: Only jump when space is held, gate with timing
    void update() {
        if (!isEnabled || localPlayerPawn == 0) {
            return;
        }

        // Read space key - this is the PRIMARY check
        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        // If space NOT held, early exit - don't jump
        if (!spacePressed) {
            lastSpaceState = false;
            return;
        }

        // Space IS held. Check if we already jumped recently (50ms gate)
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastJump = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastJumpTime).count();

        if (timeSinceLastJump < JUMP_INTERVAL_MS) {
            return;  // Not enough time passed, skip this frame
        }

        // Get dynamic offsets
        const auto& offsets = OffsetsManager::Get();

        // Read player flags to check if on ground - DYNAMIC
        uint32_t flags = drv.read<uint32_t>(localPlayerPawn + offsets.m_fFlags);
        bool isOnGround = (flags & PlayerFlags::FL_ONGROUND) != 0;

        ConsoleLogger::logBhopUpdate(spacePressed, isOnGround, flags);

        // Execute jump (whether on ground or in air)
        if (isOnGround) {
            // On ground: Use normal button press for animation
            performJump();
        }
        else {
            // IN AIR: Use KERNEL POWER to force velocity upward
            forceJumpVelocity();
        }

        lastJumpTime = now;
        lastSpaceState = true;
        jumpCount++;
    }

private:
    void performJump() {
        // Get jump button address - DYNAMIC
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t jumpAddress = clientBase + offsets.buttons.jump;

        // Press jump (0x10001 = bit 0 + bit 16)
        uint32_t pressValue = 65537;
        drv.write_memory(reinterpret_cast<uint32_t*>(jumpAddress), &pressValue, sizeof(pressValue));
        ConsoleLogger::logBhopMemoryWrite(jumpAddress, pressValue, "Jump PRESS");

        // Minimal delay
        std::this_thread::sleep_for(std::chrono::microseconds(500));

        // Release jump (0x100)
        uint32_t releaseValue = 256;
        drv.write_memory(reinterpret_cast<uint32_t*>(jumpAddress), &releaseValue, sizeof(releaseValue));
        ConsoleLogger::logBhopMemoryWrite(jumpAddress, releaseValue, "Jump RELEASE");
    }

    void forceJumpVelocity() {
        // Get dynamic offsets
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t velocityAddress = localPlayerPawn + offsets.m_vecAbsVelocity;

        // Write velocity as 3 floats: X, Y, Z (Z = up/down)
        struct Vec3_f {
            float x, y, z;
        };

        // Get current velocity
        Vec3_f currentVel = drv.read<Vec3_f>(velocityAddress);

        // Keep horizontal velocity, ADD vertical jump velocity
        currentVel.z += static_cast<float>(JUMP_VELOCITY);

        // Write back
        drv.write_memory(reinterpret_cast<Vec3_f*>(velocityAddress), &currentVel, sizeof(Vec3_f));

        if (jumpCount % 10 == 0) {  // Log every 10 frames to avoid spam
            ConsoleLogger::logBhopMemoryWrite(velocityAddress, static_cast<uint32_t>(currentVel.z), "Force velocity (kernel)");
        }
    }
};