#pragma once

#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include "src/Core/process.hpp"
#include "src/Helpers/console_colors.hpp"
#include "src/Helpers/logger.hpp"
#include "app_state.hpp"

namespace ProcessUtils {
    inline DWORD WaitForProcess(const std::wstring& name) {
        std::cout << "[*] Waiting for " << std::string(name.begin(), name.end()) << "...";
        std::cout.flush();
        while (true) {
            DWORD pid = process::get_process_id_by_name(name);
            if (pid != 0) {
                std::cout << "\r[+] Found " << std::string(name.begin(), name.end())
                          << " (PID: " << pid << ")          \n";
                return pid;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    inline void CS2ProcessMonitor() {
        while (!g_shutdownRequested.load()) {
            if (g_cs2ProcessId != 0) {
                DWORD pid = process::get_process_id_by_name(L"cs2.exe");
                if (pid == 0) {
                    g_cs2ProcessRunning.store(false);
                    ConsoleColor::PrintWarning("CS2 process terminated!");
                    Logger::LogWarning("CS2 process terminated - PID " + std::to_string(g_cs2ProcessId) + " no longer exists");
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}
