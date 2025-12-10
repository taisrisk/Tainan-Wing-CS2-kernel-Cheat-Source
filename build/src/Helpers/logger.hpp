#pragma once

#include <fstream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <windows.h>

namespace Logger {
    // Global logger state
    static std::ofstream logFile;
    static std::mutex logMutex;
    static bool initialized = false;
    static std::string currentSessionStart;

    // Get executable directory
    inline std::string GetExecutableDirectory() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string fullPath(buffer);
        size_t pos = fullPath.find_last_of("\\/");
        return fullPath.substr(0, pos);
    }

    // Get current timestamp
    inline std::string GetTimestamp() {
        auto now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        std::ostringstream oss;
        oss << std::put_time(&timeinfo, "%H:%M:%S");
        return oss.str();
    }

    // Get current date
    inline std::string GetDate() {
        auto now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        std::ostringstream oss;
        oss << std::put_time(&timeinfo, "%Y-%m-%d");
        return oss.str();
    }

    // Get current date and time for session header
    inline std::string GetDateTime() {
        auto now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        std::ostringstream oss;
        oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // Initialize logger
    inline bool Initialize() {
        std::lock_guard<std::mutex> lock(logMutex);
        
        if (initialized) {
            return true;
        }

        std::string exeDir = GetExecutableDirectory();
        std::string logPath = exeDir + "\\Logs.txt";

        // Open in append mode
        logFile.open(logPath, std::ios::app);
        
        if (!logFile.is_open()) {
            return false;
        }

        initialized = true;
        currentSessionStart = GetDateTime();

        // Write session header
        logFile << "\n";
        logFile << "================================================================================\n";
        logFile << "  SESSION START: " << currentSessionStart << "\n";
        logFile << "================================================================================\n";
        logFile << "\n";
        logFile.flush();

        return true;
    }

    // Write to log file
    inline void Write(const std::string& message) {
        if (!initialized) {
            return;
        }

        std::lock_guard<std::mutex> lock(logMutex);
        logFile << message;
        logFile.flush();
    }

    // Write with timestamp
    inline void Log(const std::string& message) {
        Write("[" + GetTimestamp() + "] " + message + "\n");
    }

    // Log with prefix
    inline void LogInfo(const std::string& message) {
        Log("[*] " + message);
    }

    inline void LogSuccess(const std::string& message) {
        Log("[+] " + message);
    }

    inline void LogError(const std::string& message) {
        Log("[-] " + message);
    }

    inline void LogWarning(const std::string& message) {
        Log("[!] " + message);
    }

    // Log section header
    inline void LogHeader(const std::string& title) {
        Write("\n");
        Write("========================================\n");
        Write("  " + title + "\n");
        Write("========================================\n");
    }

    // Close logger
    inline void Shutdown() {
        std::lock_guard<std::mutex> lock(logMutex);
        
        if (initialized && logFile.is_open()) {
            logFile << "\n";
            logFile << "================================================================================\n";
            logFile << "  SESSION END: " << GetDateTime() << "\n";
            logFile << "  Duration: Session started at " << currentSessionStart << "\n";
            logFile << "================================================================================\n";
            logFile << "\n";
            logFile.close();
            initialized = false;
        }
    }
}
