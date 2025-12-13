#pragma once
#include <atomic>
#include <cstdint>
#include "../Core/driver.hpp"
#include "../Features/ImGuiESP.hpp"
#include "features_init.hpp"

namespace LoopRunner {
    void Run(driver::DriverHandle& drv, std::uintptr_t client, ImGuiESP& esp,
             Features& f, Toggles& t, std::atomic<bool>& cs2Running);
}
