#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "entity.hpp"
#include <cstdint>

// Chams: Model highlighting through ESP overlay (not memory-based glow)
// This version just marks entities for ESP to render differently
class Chams {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    bool enabled = false;

public:
    Chams(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client) {}

    void setEnabled(bool enable) { 
        enabled = enable; 
    }

    bool isEnabled() const { 
        return enabled; 
    }

    // Update chams - this is now just a passthrough
    // The actual rendering is handled by ImGuiESP with different colors
    void update(const EntityManager& entities, std::uintptr_t localPlayerAddress) {
        // Chams rendering is handled in ESP overlay
        // This function exists for consistency but doesn't need to do memory writes
    }
};
