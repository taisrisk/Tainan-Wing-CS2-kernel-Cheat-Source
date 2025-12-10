#pragma once

#include <atomic>
#include <windows.h>

extern std::atomic<bool> g_cs2ProcessRunning;
extern std::atomic<bool> g_shutdownRequested;
extern DWORD g_cs2ProcessId;

inline void ResetAppState() {
    g_cs2ProcessRunning.store(true);
    g_shutdownRequested.store(false);
    g_cs2ProcessId = 0;
}
