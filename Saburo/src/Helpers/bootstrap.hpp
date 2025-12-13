#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include "version_manager.hpp"
#include "console_colors.hpp"
#include "logger.hpp"
#include "../Core/driver.hpp"

namespace Bootstrap {
    bool InitializeLogger();
    bool VersionCheckAndEnsureFiles(VersionManager::VersionConfig& outConfig);
    bool ConnectOrLoadDriver(driver::DriverHandle& driver, const VersionManager::VersionConfig& cfg);
    DWORD WaitForProcess(const std::wstring& name);
    void StartProcessMonitor(std::atomic<bool>& runningFlag, std::atomic<bool>& shutdownFlag, DWORD& pidRef);
    bool AttachToProcess(driver::DriverHandle& driver, DWORD pid);
    bool WaitForClientDll(driver::DriverHandle& driver, DWORD pid, std::uintptr_t& outClientBase);
}
