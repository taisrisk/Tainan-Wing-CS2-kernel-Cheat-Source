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
    bool lastGroundState;
    bool lastSpaceState;

    static constexpr int JUMP_VELOCITY = 280;
    static constexpr int JUMP_INTERVAL_MS = 50;
    static constexpr int MIN_GROUND_TIME_MS = 10; // Minimum time on ground before next jump

public:
    BunnyHop(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), localPlayerPawn(0), isEnabled(false), 
          jumpCount(0), lastGroundState(false), lastSpaceState(false) {
        lastJumpTime = std::chrono::steady_clock::now();
    }

    void setEnabled(bool enabled) {
        isEnabled = enabled;
        jumpCount = 0;
        lastSpaceState = false;
        lastGroundState = false;
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

    // Main update - OPTIMIZED timing
    void update() {
        if (!isEnabled || localPlayerPawn == 0) {
            return;
        }

        // Read space key - PRIMARY check
        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        // If space NOT held, reset and exit
        if (!spacePressed) {
            lastSpaceState = false;
            return;
        }

        // Check timing gate (50ms minimum between jumps)
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastJump = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastJumpTime).count();

        if (timeSinceLastJump < JUMP_INTERVAL_MS) {
            return;  // Too soon, skip this frame
        }

        // Get dynamic offsets
        const auto& offsets = OffsetsManager::Get();

        // Read player flags to check if on ground
        uint32_t flags = drv.read<uint32_t>(localPlayerPawn + offsets.m_fFlags);
        bool isOnGround = (flags & PlayerFlags::FL_ONGROUND) != 0;

        // Log updates occasionally
        ConsoleLogger::logBhopUpdate(spacePressed, isOnGround, flags);

        // State transition: was in air, now on ground
        bool justLanded = !lastGroundState && isOnGround;
        
        // If just landed, wait minimum ground time before jumping again
        if (justLanded) {
            lastJumpTime = now; // Reset timer on landing
            lastGroundState = true;
            return;
        }

        // Execute jump when on ground
        if (isOnGround) {
            performJump();
            lastJumpTime = now;
            lastGroundState = false; // We're jumping, so not on ground anymore
            jumpCount++;
        }
        else {
            // In air - apply slight upward velocity boost (optional)
            if (jumpCount % 5 == 0) { // Only every 5th frame in air
                applyAirVelocityBoost();
            }
            lastGroundState = false;
        }

        lastSpaceState = true;
    }

private:
    void performJump() {
        // Get jump button address
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t jumpAddress = clientBase + offsets.buttons.jump;

        // Press jump (0x10001 = bit 0 + bit 16)
        uint32_t pressValue = 65537;
        drv.write_memory(reinterpret_cast<uint32_t*>(jumpAddress), &pressValue, sizeof(pressValue));
        ConsoleLogger::logBhopMemoryWrite(jumpAddress, pressValue, "Jump PRESS");

        // Minimal delay (500 microseconds)
        std::this_thread::sleep_for(std::chrono::microseconds(500));

        // Release jump (0x100)
        uint32_t releaseValue = 256;
        drv.write_memory(reinterpret_cast<uint32_t*>(jumpAddress), &releaseValue, sizeof(releaseValue));
        ConsoleLogger::logBhopMemoryWrite(jumpAddress, releaseValue, "Jump RELEASE");
    }

    void applyAirVelocityBoost() {
        // Get dynamic offsets
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t velocityAddress = localPlayerPawn + offsets.m_vecAbsVelocity;

        // Write velocity as 3 floats: X, Y, Z (Z = up/down)
        struct Vec3_f {
            float x, y, z;
        };

        // Get current velocity
        Vec3_f currentVel = drv.read<Vec3_f>(velocityAddress);

        // Only boost if we're falling (negative Z velocity)
        if (currentVel.z < 0) {
            // Small upward boost (not as strong as jump)
            currentVel.z += 50.0f;

            // Write back
            drv.write_memory(reinterpret_cast<Vec3_f*>(velocityAddress), &currentVel, sizeof(Vec3_f));

            if (jumpCount % 30 == 0) {  // Log occasionally
                ConsoleLogger::logBhopMemoryWrite(velocityAddress, static_cast<uint32_t>(currentVel.z), "Air velocity boost");
            }
        }
    }
};