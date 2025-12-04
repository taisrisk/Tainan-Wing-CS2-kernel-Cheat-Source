#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <string>
#include <windows.h>

// Define missing color constants
#ifndef FOREGROUND_YELLOW
#define FOREGROUND_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)
#endif

#ifndef FOREGROUND_CYAN
#define FOREGROUND_CYAN (FOREGROUND_GREEN | FOREGROUND_BLUE)
#endif

#ifndef FOREGROUND_MAGENTA
#define FOREGROUND_MAGENTA (FOREGROUND_RED | FOREGROUND_BLUE)
#endif

// Singleton debug console logger
class ConsoleLogger {
private:
    static bool debugEnabled;
    static HANDLE debugConsoleHandle;
    static FILE* debugConsoleStream;
    static int logCount;
    static const int MAX_LOGS_PER_SECOND = 10; // Prevent console flood
    static std::chrono::steady_clock::time_point lastResetTime;
    
    ConsoleLogger() = default;
    
    static bool shouldLog() {
        if (!debugEnabled) return false;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastResetTime).count();
        
        if (elapsed >= 1) {
            logCount = 0;
            lastResetTime = now;
        }
        
        if (logCount >= MAX_LOGS_PER_SECOND) {
            return false;
        }
        
        logCount++;
        return true;
    }
    
    static std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        
        std::tm bt{};
        localtime_s(&bt, &timer);
        
        std::ostringstream oss;
        oss << std::put_time(&bt, "%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
    
    static void createDebugConsole() {
        if (debugConsoleHandle != INVALID_HANDLE_VALUE) {
            return; // Already created
        }
        
        // Allocate a new console window
        if (!AllocConsole()) {
            return; // Failed to create
        }
        
        // Get handle to new console (don't redirect main stdout!)
        debugConsoleHandle = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        
        if (debugConsoleHandle == INVALID_HANDLE_VALUE) {
            FreeConsole();
            return;
        }
        
        // Set console title
        SetConsoleTitleA("Subaro - Debug Console");
        
        // Set console size
        COORD newSize;
        newSize.X = 120;
        newSize.Y = 3000;
        SetConsoleScreenBufferSize(debugConsoleHandle, newSize);
        
        // Set window size
        SMALL_RECT windowSize;
        windowSize.Left = 0;
        windowSize.Top = 0;
        windowSize.Right = 119;
        windowSize.Bottom = 29;
        SetConsoleWindowInfo(debugConsoleHandle, TRUE, &windowSize);
        
        // Print header to debug console
        DWORD written;
        const char* header = "========================================\n  SUBARO DEBUG CONSOLE\n========================================\nDebug output will appear here.\nPress [0] in main console to toggle.\n\n";
        WriteConsoleA(debugConsoleHandle, header, (DWORD)strlen(header), &written, NULL);
    }
    
    static void closeDebugConsole() {
        if (debugConsoleHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(debugConsoleHandle);
            debugConsoleHandle = INVALID_HANDLE_VALUE;
            FreeConsole();
        }
    }
    
    static void writeToDebugConsole(const char* text, WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE) {
        if (debugConsoleHandle == INVALID_HANDLE_VALUE) return;
        
        SetConsoleTextAttribute(debugConsoleHandle, color);
        DWORD written;
        WriteConsoleA(debugConsoleHandle, text, (DWORD)strlen(text), &written, NULL);
    }
    
public:
    static void setEnabled(bool enabled) {
        debugEnabled = enabled;
        
        if (enabled) {
            createDebugConsole();
        } else {
            closeDebugConsole();
        }
    }
    
    static bool isEnabled() {
        return debugEnabled;
    }
    
    // TRIGGERBOT LOGGING
    static void logTriggerUpdate(bool hasTarget, uint32_t crosshairID) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[TB:%s] Update - CrosshairID: 0x%X | HasTarget: %s\n",
                 getTimestamp().c_str(), crosshairID, hasTarget ? "YES" : "NO");
        writeToDebugConsole(buffer, FOREGROUND_CYAN | FOREGROUND_INTENSITY);
    }
    
    static void logTriggerTargetFound(uint8_t enemyTeam, uint8_t localTeam, std::uintptr_t targetAddr) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[TB:%s] TARGET FOUND - Addr: 0x%llX | Team: %d vs %d\n",
                 getTimestamp().c_str(), (unsigned long long)targetAddr, (int)enemyTeam, (int)localTeam);
        writeToDebugConsole(buffer, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    
    static void logTriggerTargetTeammate(uint8_t team) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[TB:%s] Teammate detected (Team %d) - Skipping\n",
                 getTimestamp().c_str(), (int)team);
        writeToDebugConsole(buffer, FOREGROUND_YELLOW | FOREGROUND_INTENSITY);
    }
    
    static void logTriggerShot(long long timeSinceLastShot, long long timeSinceFound) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[TB:%s] FIRING SHOT! LastShot: %lldms | TargetAge: %lldms\n",
                 getTimestamp().c_str(), timeSinceLastShot, timeSinceFound);
        writeToDebugConsole(buffer, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
    
    static void logTriggerMemoryWrite(std::uintptr_t address, uint32_t value, bool success) {
        if (!shouldLog()) return;
        
        char buffer[256];
        if (success) {
            snprintf(buffer, sizeof(buffer), "[TB:%s] Memory Write - Addr: 0x%llX | Value: %u\n",
                     getTimestamp().c_str(), (unsigned long long)address, value);
            writeToDebugConsole(buffer, FOREGROUND_GREEN);
        } else {
            snprintf(buffer, sizeof(buffer), "[TB:%s] FAILED Write - Addr: 0x%llX\n",
                     getTimestamp().c_str(), (unsigned long long)address);
            writeToDebugConsole(buffer, FOREGROUND_RED | FOREGROUND_INTENSITY);
        }
    }
    
    static void logTriggerNoTarget(const char* reason) {
        static int skipCount = 0;
        skipCount++;
        
        if (skipCount % 100 != 0) return;
        if (!debugEnabled) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[TB:%s] No target (%s) - Skipped x%d\n",
                 getTimestamp().c_str(), reason, skipCount);
        writeToDebugConsole(buffer, FOREGROUND_BLUE);
        skipCount = 0;
    }
    
    // BHOP LOGGING
    static void logBhopUpdate(bool spacePressed, bool onGround, uint32_t flags) {
        static int frameCount = 0;
        frameCount++;
        
        if (!spacePressed && frameCount % 60 != 0) return;
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[BH:%s] Update - Space: %s | OnGround: %s | Flags: 0x%X\n",
                 getTimestamp().c_str(), spacePressed ? "YES" : "NO", onGround ? "YES" : "NO", flags);
        writeToDebugConsole(buffer, FOREGROUND_MAGENTA | FOREGROUND_INTENSITY);
        
        if (!spacePressed) frameCount = 0;
    }
    
    static void logBhopJump(long long timeSinceLastJump) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[BH:%s] JUMPING! TimeSinceLast: %lldms\n",
                 getTimestamp().c_str(), timeSinceLastJump);
        writeToDebugConsole(buffer, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    
    static void logBhopMemoryWrite(std::uintptr_t address, uint32_t value, const char* action) {
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[BH:%s] %s - Addr: 0x%llX | Value: %u\n",
                 getTimestamp().c_str(), action, (unsigned long long)address, value);
        writeToDebugConsole(buffer, FOREGROUND_CYAN);
    }
    
    static void logBhopInAir() {
        static int airCount = 0;
        airCount++;
        
        if (airCount % 30 != 0) return;
        if (!shouldLog()) return;
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "[BH:%s] In air - Waiting for landing... (x%d)\n",
                 getTimestamp().c_str(), airCount);
        writeToDebugConsole(buffer, FOREGROUND_YELLOW);
        airCount = 0;
    }
    
    static void logBhopFlagRead(std::uintptr_t address, uint32_t flags, bool success) {
        static int readCount = 0;
        readCount++;
        
        if (readCount % 120 != 0) return;
        if (!shouldLog()) return;
        
        char buffer[256];
        if (success) {
            snprintf(buffer, sizeof(buffer), "[BH:%s] Flag Read - Addr: 0x%llX | Flags: 0x%X\n",
                     getTimestamp().c_str(), (unsigned long long)address, flags);
            writeToDebugConsole(buffer, FOREGROUND_GREEN);
        } else {
            snprintf(buffer, sizeof(buffer), "[BH:%s] FAILED Flag Read - Addr: 0x%llX\n",
                     getTimestamp().c_str(), (unsigned long long)address);
            writeToDebugConsole(buffer, FOREGROUND_RED);
        }
    }
    
    // GENERAL LOGGING
    static void logError(const char* feature, const char* message) {
        if (!debugEnabled) return;
        
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "[%s:%s] ERROR: %s\n",
                 feature, getTimestamp().c_str(), message);
        writeToDebugConsole(buffer, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
    
    static void logInfo(const char* feature, const char* message) {
        if (!debugEnabled) return;
        
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "[%s:%s] INFO: %s\n",
                 feature, getTimestamp().c_str(), message);
        writeToDebugConsole(buffer, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }
};
