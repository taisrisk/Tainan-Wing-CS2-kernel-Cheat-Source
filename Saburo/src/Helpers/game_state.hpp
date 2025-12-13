#pragma once
#include <cstdint>
#include <atomic>
#include "../Features/ImGuiESP.hpp"
#include "../Core/driver.hpp"

namespace GameState {
    bool WaitForInGame(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& espRenderer,
                       std::atomic<bool>& cs2Running);
}
