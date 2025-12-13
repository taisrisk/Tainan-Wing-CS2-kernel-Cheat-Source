#pragma once

#include "entity.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include "../Offsets/buttons.hpp"
#include "../Helpers/console_logger.hpp"
#include <chrono>
#include <thread>
#include <windows.h>

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
    std::chrono::steady_clock::time_point lastActionTime;
    bool jumpActive;
    DWORD targetProcessId = 0; // Foreground enforcement
    int jumpDelayMs = 10;      // Default C# behavior
    bool strafeLeft = false;
    std::chrono::steady_clock::time_point lastStrafeToggle;
    int strafePeriodMs = 20;

    static constexpr int JUMP_VELOCITY = 280;
    static constexpr int JUMP_INTERVAL_MS = 50;

    // Keyboard fallback using SendInput when memory writes fail
    void keyDown(WORD vk) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        SendInput(1, &in, sizeof(INPUT));
    }
    void keyUp(WORD vk) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    }

public:
    BunnyHop(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), localPlayerPawn(0), isEnabled(false), jumpCount(0), lastSpaceState(false),
          jumpActive(false), targetProcessId(0), jumpDelayMs(10), strafeLeft(false), strafePeriodMs(20) {
        lastJumpTime = std::chrono::steady_clock::now();
        lastActionTime = std::chrono::steady_clock::now();
        lastStrafeToggle = std::chrono::steady_clock::now();
    }

    void setEnabled(bool enabled) {
        isEnabled = enabled;
        jumpActive = false;
        if (ConsoleLogger::isEnabled()) {
            ConsoleLogger::logInfo("BH", enabled ? "ENABLED" : "DISABLED");
        }
        // On disable: release jump and strafe keys to avoid stuck inputs
        if (!enabled && clientBase != 0) {
            const auto& offsets = OffsetsManager::Get();
            uint32_t releaseValue = 256; // 0x100
            if (offsets.buttons.jump) {
                std::uintptr_t jumpAddress = clientBase + offsets.buttons.jump;
                drv.write_memory(reinterpret_cast<uint32_t*>(jumpAddress), &releaseValue, sizeof(releaseValue));
            }
            if (offsets.buttons.left) {
                std::uintptr_t leftAddress = clientBase + offsets.buttons.left;
                drv.write_memory(reinterpret_cast<uint32_t*>(leftAddress), &releaseValue, sizeof(releaseValue));
            }
            if (offsets.buttons.right) {
                std::uintptr_t rightAddress = clientBase + offsets.buttons.right;
                drv.write_memory(reinterpret_cast<uint32_t*>(rightAddress), &releaseValue, sizeof(releaseValue));
            }
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

    void setTargetProcessId(DWORD pid) { targetProcessId = pid; }
    void setJumpDelayMs(int ms) { jumpDelayMs = (ms < 1 ? 1 : ms); }

public:
    // Main update - OPTIMIZED timing
    void update() {
        if (!isEnabled || localPlayerPawn == 0) {
            return;
        }

        // Enforce active window like the C# version
        if (targetProcessId != 0) {
            HWND fg = GetForegroundWindow();
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            if (pid != targetProcessId) {
                // ensure jump inactive if toggled
                if (jumpActive) {
                    const auto& offsets = OffsetsManager::Get();
                    std::uintptr_t jumpAddress = clientBase + offsets.buttons.jump;
                    std::uintptr_t leftAddress = clientBase + offsets.buttons.left;
                    std::uintptr_t rightAddress = clientBase + offsets.buttons.right;
                    uint32_t releaseValue = 256; // 0x100
                    bool okJ = drv.write_memory(reinterpret_cast<void*>(jumpAddress), &releaseValue, sizeof(releaseValue));
                    bool okL = drv.write_memory(reinterpret_cast<void*>(leftAddress), &releaseValue, sizeof(releaseValue));
                    bool okR = drv.write_memory(reinterpret_cast<void*>(rightAddress), &releaseValue, sizeof(releaseValue));
                    if (!okJ) keyUp(VK_SPACE);
                    if (!okL) keyUp('A');
                    if (!okR) keyUp('D');
                    jumpActive = false;
                }
                return;
            }
        }

        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        bool userLeftHeld  = (GetAsyncKeyState('A') & 0x8000) != 0;
        bool userRightHeld = (GetAsyncKeyState('D') & 0x8000) != 0;
        const auto& offsets = OffsetsManager::Get();
        std::uintptr_t jumpAddress = clientBase + offsets.buttons.jump;
        std::uintptr_t leftAddress = clientBase + offsets.buttons.left;
        std::uintptr_t rightAddress = clientBase + offsets.buttons.right;

        auto now = std::chrono::steady_clock::now();
        auto strafeSinceMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStrafeToggle).count();

        if (!spacePressed) {
            // If key not held, ensure inactive and reset state
            if (jumpActive) {
                uint32_t releaseValue = 256; // 0x100
                bool ok = drv.write_memory(reinterpret_cast<void*>(jumpAddress), &releaseValue, sizeof(releaseValue));
                if (!ok) keyUp(VK_SPACE);
                jumpActive = false;
            }
            // Do not touch A/D when SPACE is not held
            return;
        }

        // Read on-ground flag
        uint32_t flags = drv.read<uint32_t>(localPlayerPawn + offsets.m_fFlags);
        bool onGround = (flags & PlayerFlags::FL_ONGROUND) != 0;

        uint32_t pressValue = 65537;   // 0x10001
        uint32_t releaseValue = 256;   // 0x100

        if (onGround) {
            // No strafing on ground (leave user's A/D alone)
            // Continuous bhop: press on ground, release in air
            bool pOk = drv.write_memory(reinterpret_cast<void*>(jumpAddress), &pressValue, sizeof(pressValue));
            bool rOk = drv.write_memory(reinterpret_cast<void*>(jumpAddress), &releaseValue, sizeof(releaseValue));
            if (!pOk || !rOk) {
                keyDown(VK_SPACE);
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                keyUp(VK_SPACE);
            }
            return;
        }

        // In air: auto-strafe alternating left/right unless user is holding A/D or aiming (RMB)
        bool rmbHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (userLeftHeld || userRightHeld || rmbHeld) {
            // Let user control strafe; do not touch A/D
            return;
        }

        // In air: auto-strafe alternating left/right
        if (strafeSinceMs >= strafePeriodMs) {
            strafeLeft = !strafeLeft;
            lastStrafeToggle = now;
        }

        if (strafeLeft) {
            bool lp = drv.write_memory(reinterpret_cast<void*>(leftAddress), &pressValue, sizeof(pressValue));
            bool rr = drv.write_memory(reinterpret_cast<void*>(rightAddress), &releaseValue, sizeof(releaseValue));
            if (!lp) keyDown('A');
            if (!rr) keyUp('D');
        } else {
            bool rp = drv.write_memory(reinterpret_cast<void*>(rightAddress), &pressValue, sizeof(pressValue));
            bool lr = drv.write_memory(reinterpret_cast<void*>(leftAddress), &releaseValue, sizeof(releaseValue));
            if (!rp) keyDown('D');
            if (!lr) keyUp('A');
        }
    }

private:
    // Intentionally removed complex ground checks for 1:1 C# behavior
};