#include "esp_setup.hpp"
#include "../Helpers/OS-ImGui/OS-ImGui.h"
#include "console_colors.hpp"
#include <thread>
#include <chrono>
#include <atomic>

namespace ESPSetup {

bool StartOverlay(driver::DriverHandle& drv, std::uintptr_t clientBase, ImGuiESP& outRenderer) {
    ConsoleColor::PrintInfo("Initializing ESP overlay...");
    // outRenderer is already constructed by the caller; avoid copy-assignment (non-copyable)

    std::atomic<bool> espRunning(false);
    std::thread espThread([&]() {
        try {
            espRunning = true;
            OSImGui::OSImGui gui;
            gui.AttachAnotherWindow("Counter-Strike 2", "", [&]() {
                outRenderer.updateViewMatrix();
                outRenderer.render();
            });
        } catch (...) {
            espRunning = false;
        }
    });
    espThread.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!espRunning) {
        ConsoleColor::PrintError("Failed to initialize ESP overlay");
        return false;
    }
    ConsoleColor::PrintSuccess("ESP overlay initialized");
    return true;
}

}
