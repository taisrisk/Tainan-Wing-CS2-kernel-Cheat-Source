#include "console_logger.hpp"

// Static member definitions
bool ConsoleLogger::debugEnabled = false;
HANDLE ConsoleLogger::debugConsoleHandle = INVALID_HANDLE_VALUE;
FILE* ConsoleLogger::debugConsoleStream = nullptr;
int ConsoleLogger::logCount = 0;
std::chrono::steady_clock::time_point ConsoleLogger::lastResetTime = std::chrono::steady_clock::now();
