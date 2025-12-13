#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include <array>
#include <cstdint>

// Third-person camera system for CS2
// Patches the game to allow third-person view
class ThirdPerson {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    bool enabled;
    bool patchApplied;

    // Offset for the CMP instruction that checks third-person state
    static constexpr std::uintptr_t CMP_PATCH_OFFSET = 0x8183B4;

    // Original bytes: cmp [rax],r12b
    static constexpr std::array<std::uint8_t, 3> ORIGINAL_CMP = { 0x44, 0x38, 0x20 };

    // Patched bytes: NOP NOP NOP (disable the check)
    static constexpr std::array<std::uint8_t, 3> PATCHED_CMP = { 0x90, 0x90, 0x90 };

    // dwCSGOInput offset for third-person state
    static constexpr std::uintptr_t THIRDPERSON_STATE_OFFSET = 0x251;

public:
    ThirdPerson(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), enabled(false), patchApplied(false) {}

    void setEnabled(bool enable) {
        if (enable && !enabled) {
            applyPatch();
            setState(true);
            enabled = true;
        }
        else if (!enable && enabled) {
            removePatch();
            setState(false);
            enabled = false;
        }
    }

    bool isEnabled() const {
        return enabled;
    }

    // Update third-person state (call each frame)
    void update(std::uintptr_t localPlayerPawn) {
        if (!enabled || localPlayerPawn == 0) {
            return;
        }

        // Re-apply patch bytes if the instruction is restored
        if (!isPatchActive()) {
            applyPatch();
        }

        // Check if third-person state needs reapply (game might reset it)
        if (needsReapply()) {
            setState(true);
        }
    }

private:
    void applyPatch() {
        if (patchApplied) {
            return;
        }

        std::uintptr_t patchAddress = clientBase + CMP_PATCH_OFFSET;
        std::array<std::uint8_t, 3> cur{};
        drv.read_memory(reinterpret_cast<void*>(patchAddress), cur.data(), cur.size());
        // Only patch if current matches original or already patched
        if (!(cur == ORIGINAL_CMP || cur == PATCHED_CMP)) {
            enabled = false;
            patchApplied = false;
            return;
        }
        bool ok = drv.write_memory(reinterpret_cast<void*>(patchAddress),
            const_cast<std::uint8_t*>(PATCHED_CMP.data()), PATCHED_CMP.size());
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

        std::uintptr_t patchAddress = clientBase + CMP_PATCH_OFFSET;
        std::array<std::uint8_t, 3> cur{};
        drv.read_memory(reinterpret_cast<void*>(patchAddress), cur.data(), cur.size());
        if (cur == PATCHED_CMP) {
            bool ok = drv.write_memory(reinterpret_cast<void*>(patchAddress),
                const_cast<std::uint8_t*>(ORIGINAL_CMP.data()), ORIGINAL_CMP.size());
            (void)ok; // ignore failure on teardown
        }
        patchApplied = false;
    }

    void setState(bool state) {
        std::uintptr_t inputAddress = clientBase + OffsetsManager::Get().dwCSGOInput + THIRDPERSON_STATE_OFFSET;
        std::uint8_t value = state ? 1 : 0;
        bool ok = drv.write_memory(reinterpret_cast<void*>(inputAddress), &value, sizeof(value));
        if (!ok) {
            enabled = false;
            patchApplied = false;
        }
    }

    bool needsReapply() {
        if (!enabled) {
            return false;
        }

        // Check if game has reset the third-person state
        std::uintptr_t inputAddress = clientBase + OffsetsManager::Get().dwCSGOInput + THIRDPERSON_STATE_OFFSET;
        std::uint8_t currentValue = drv.read<std::uint8_t>(inputAddress);

        return currentValue == 0;
    }

    bool isPatchActive() {
        std::uintptr_t addr = clientBase + CMP_PATCH_OFFSET;
        std::array<std::uint8_t, 3> cur{};
        drv.read_memory(reinterpret_cast<void*>(addr), cur.data(), cur.size());
        return cur == PATCHED_CMP;
    }
};
