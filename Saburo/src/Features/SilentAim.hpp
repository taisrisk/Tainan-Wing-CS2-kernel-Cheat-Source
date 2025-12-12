#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include <array>
#include <cstdint>

// Silent Aim system for CS2
// Patches dwViewAngles to prevent it from overwriting v_angle
// This allows bullets to go to the target without visually changing view angles
class SilentAim {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    bool enabled;
    bool patchApplied;

    // Offset for the view angles patch
    static constexpr std::uintptr_t VANGLES_PATCH_OFFSET = 0x7D8950;

    // Original bytes: movsd [rcx+14A8h], xmm0
    static constexpr std::array<std::uint8_t, 8> ORIGINAL_VANGLES = { 0xF2, 0x0F, 0x11, 0x81, 0xA8, 0x14, 0x00, 0x00 };

    // Patched bytes: NOP out the instruction
    static constexpr std::array<std::uint8_t, 8> PATCHED_VANGLES = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

public:
    SilentAim(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), enabled(false), patchApplied(false) {}

    void setEnabled(bool enable) {
        if (enable && !enabled) {
            applyPatch();
            enabled = true;
        }
        else if (!enable && enabled) {
            removePatch();
            enabled = false;
        }
    }

    bool isEnabled() const {
        return enabled;
    }

    // Update silent aim state (call each frame)
    void update() {
        // Silent aim patch is persistent once applied
        // Only reapply if game has somehow restored original bytes
        if (enabled && !isPatchActive()) {
            applyPatch();
        }
    }

private:
    void applyPatch() {
        if (patchApplied) {
            return;
        }

        std::uintptr_t patchAddress = clientBase + VANGLES_PATCH_OFFSET;
        std::array<std::uint8_t, 8> currentBytes{};
        drv.read_memory(reinterpret_cast<void*>(patchAddress), currentBytes.data(), currentBytes.size());
        // Only patch if current matches original or already patched
        if (!(currentBytes == ORIGINAL_VANGLES || currentBytes == PATCHED_VANGLES)) {
            enabled = false;
            patchApplied = false;
            return;
        }
        bool ok = drv.write_memory(reinterpret_cast<void*>(patchAddress),
            const_cast<std::uint8_t*>(PATCHED_VANGLES.data()), PATCHED_VANGLES.size());
        if (!ok) {
            enabled = false;
            patchApplied = false;
            return;
        }
        patchApplied = true;
    }

    void removePatch() {
        if (!patchApplied) {
            return;
        }

        std::uintptr_t patchAddress = clientBase + VANGLES_PATCH_OFFSET;
        std::array<std::uint8_t, 8> currentBytes{};
        drv.read_memory(reinterpret_cast<void*>(patchAddress), currentBytes.data(), currentBytes.size());
        if (currentBytes == PATCHED_VANGLES) {
            bool ok = drv.write_memory(reinterpret_cast<void*>(patchAddress),
                const_cast<std::uint8_t*>(ORIGINAL_VANGLES.data()), ORIGINAL_VANGLES.size());
            (void)ok; // ignore failure on teardown
        }
        patchApplied = false;
    }

    bool isPatchActive() {
        std::uintptr_t patchAddress = clientBase + VANGLES_PATCH_OFFSET;
        std::array<std::uint8_t, 8> currentBytes;
        drv.read_memory(reinterpret_cast<void*>(patchAddress), currentBytes.data(), currentBytes.size());

        // Check if bytes match our patched version
        return currentBytes == PATCHED_VANGLES;
    }
};
