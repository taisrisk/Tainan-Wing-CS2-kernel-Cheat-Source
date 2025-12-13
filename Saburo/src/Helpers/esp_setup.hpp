#pragma once
#include <atomic>
#include <cstdint>
#include "../Features/ImGuiESP.hpp"

namespace ESPSetup {
    bool StartOverlay(driver::DriverHandle& drv, std::uintptr_t clientBase, ImGuiESP& outRenderer);
}
