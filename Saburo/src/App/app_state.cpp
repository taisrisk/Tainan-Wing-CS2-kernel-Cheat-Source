#include "app_state.hpp"

std::atomic<bool> g_cs2ProcessRunning(true);
std::atomic<bool> g_shutdownRequested(false);
DWORD g_cs2ProcessId = 0;
