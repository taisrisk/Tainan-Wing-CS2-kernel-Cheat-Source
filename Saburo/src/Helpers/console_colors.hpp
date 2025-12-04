#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ConsoleColor {
    // Color codes for Windows console
    enum Color {
        BLACK = 0,
        DARK_BLUE = 1,
        DARK_GREEN = 2,
        DARK_CYAN = 3,
        DARK_RED = 4,
        DARK_MAGENTA = 5,
        DARK_YELLOW = 6,
        GRAY = 7,
        DARK_GRAY = 8,
        BLUE = 9,
        GREEN = 10,
        CYAN = 11,
        RED = 12,
        MAGENTA = 13,
        YELLOW = 14,
        WHITE = 15
    };

    // Get console handle (static to avoid repeated calls)
    inline HANDLE GetConsoleHandle() {
        static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        return hConsole;
    }

    // Set text color
    inline void SetColor(Color foreground, Color background = BLACK) {
        WORD colorAttribute = (background << 4) | foreground;
        SetConsoleTextAttribute(GetConsoleHandle(), colorAttribute);
    }

    // Reset to default colors
    inline void Reset() {
        SetColor(GRAY, BLACK);
    }

    // Get current timestamp
    inline std::string GetTimestamp() {
        auto now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);  // Use localtime_s instead of localtime
        std::ostringstream oss;
        oss << std::put_time(&timeinfo, "%H:%M:%S");
        return oss.str();
    }

    // Print with timestamp and color
    inline void Print(const std::string& message, Color color = GRAY) {
        SetColor(DARK_GRAY);
        std::cout << "[" << GetTimestamp() << "] ";
        SetColor(color);
        std::cout << message;
        Reset();
        std::cout << "\n";
    }

    // Specialized print functions
    inline void PrintSuccess(const std::string& message) {
        SetColor(DARK_GRAY);
        std::cout << "[" << GetTimestamp() << "] ";
        SetColor(GREEN);
        std::cout << "[+] " << message;
        Reset();
        std::cout << "\n";
    }

    inline void PrintError(const std::string& message) {
        SetColor(DARK_GRAY);
        std::cout << "[" << GetTimestamp() << "] ";
        SetColor(RED);
        std::cout << "[-] " << message;
        Reset();
        std::cout << "\n";
    }

    inline void PrintWarning(const std::string& message) {
        SetColor(DARK_GRAY);
        std::cout << "[" << GetTimestamp() << "] ";
        SetColor(YELLOW);
        std::cout << "[!] " << message;
        Reset();
        std::cout << "\n";
    }

    inline void PrintInfo(const std::string& message) {
        SetColor(DARK_GRAY);
        std::cout << "[" << GetTimestamp() << "] ";
        SetColor(CYAN);
        std::cout << "[*] " << message;
        Reset();
        std::cout << "\n";
    }

    inline void PrintHeader(const std::string& title) {
        SetColor(MAGENTA);
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  " << title << "\n";
        std::cout << "========================================\n";
        Reset();
    }

    inline void PrintBox(const std::string& title, const std::vector<std::string>& lines) {
        SetColor(YELLOW);
        std::cout << "\n";
        std::cout << "+============================================+\n";
        std::cout << "|  " << title;
        // Pad to width
        size_t padding = 42 - title.length();  // Use size_t instead of int
        for (size_t i = 0; i < padding; i++) std::cout << " ";
        std::cout << "|\n";
        std::cout << "+============================================+\n";
        
        SetColor(WHITE);
        for (const auto& line : lines) {
            std::cout << "| " << line;
            // Pad to width
            size_t linePadding = 42 - line.length();  // Use size_t instead of int
            for (size_t i = 0; i < linePadding; i++) std::cout << " ";
            std::cout << "|\n";
        }
        
        SetColor(YELLOW);
        std::cout << "+============================================+\n";
        Reset();
        std::cout << "\n";
    }

    // Print feature toggle status WITHOUT NEWLINE
    inline void PrintToggle(const std::string& featureName, bool enabled) {
        // Don't print anything - status will be shown in the main display
    }

    // Print controls menu
    inline void PrintControls() {
        PrintBox("CONTROLS", {
            "",
            "  [0] Toggle Debug Console",
            "  [1] Toggle Aimbot (Hold RIGHT MOUSE)",
            "  [2] Toggle Triggerbot",
            "  [3] Toggle BunnyHop",
            "  [4] Toggle Head Line (Aim Guide)",
            "  [5] Toggle Snaplines",
            "  [6] Toggle Distance ESP",
            "  [7] Toggle Snapline Wall-Check",
            "  [8] Toggle Team Check",
            "  [9] Toggle Chams (Glow)",
            ""
        });
    }

    // Print current feature status with color coding
    inline void PrintStatus(const std::string& feature, bool enabled) {
        SetColor(CYAN);
        std::cout << "  [" << feature << "] ";
        if (enabled) {
            SetColor(GREEN);
            std::cout << "ENABLED";
        } else {
            SetColor(RED);
            std::cout << "DISABLED";
        }
        Reset();
        std::cout << "\n";
    }

    // Print all feature statuses in a clean box
    inline void PrintFeatureStatus(bool aimbot, bool triggerbot, bool bhop, bool teamCheck) {
        std::cout << "\n";
        SetColor(CYAN);
        std::cout << "+============================================+\n";
        std::cout << "|              FEATURE STATUS                |\n";
        std::cout << "+============================================+\n";
        Reset();
        
        std::cout << "  [Aimbot]      ";
        SetColor(aimbot ? GREEN : RED);
        std::cout << (aimbot ? "ENABLED " : "DISABLED");
        Reset();
        std::cout << "\n";
        
        std::cout << "  [Triggerbot]  ";
        SetColor(triggerbot ? GREEN : RED);
        std::cout << (triggerbot ? "ENABLED " : "DISABLED");
        Reset();
        std::cout << "\n";
        
        std::cout << "  [BunnyHop]    ";
        SetColor(bhop ? GREEN : RED);
        std::cout << (bhop ? "ENABLED " : "DISABLED");
        Reset();
        std::cout << "\n";
        
        std::cout << "  [Team Check]  ";
        SetColor(teamCheck ? GREEN : RED);
        std::cout << (teamCheck ? "ENABLED " : "DISABLED");
        Reset();
        std::cout << "\n";
        
        SetColor(CYAN);
        std::cout << "+============================================+\n";
        Reset();
        std::cout << "\n";
    }

    // Print ASCII logo for Saburo
    inline void PrintLogo(bool inGame = false) {
        // Blue in lobby, Red in-game
        SetColor(inGame ? RED : CYAN);
        std::cout << R"(
   _____       __                     
  / ___/__  __/ /_  ____ __________  
  \__ \/ / / / __ \/ __ `/ ___/ __ \ 
 ___/ / /_/ / /_/ / /_/ / /  / /_/ / 
/____/\__,_/_.___/\__,_/_/   \____/  
                                      
)" << "\n";
        Reset();
        SetColor(DARK_GRAY);
        std::cout << "                          By: zrorisc\n";
        Reset();
        std::cout << "\n";
    }

    // Clear console screen
    inline void ClearScreen() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD coordScreen = { 0, 0 };
        DWORD cCharsWritten;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        DWORD dwConSize;

        if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            return;
        }

        dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

        FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
        SetConsoleCursorPosition(hConsole, coordScreen);
    }
    
    // Set cursor position
    inline void SetCursorPosition(int x, int y) {
        COORD coord;
        coord.X = static_cast<SHORT>(x);
        coord.Y = static_cast<SHORT>(y);
        SetConsoleCursorPosition(GetConsoleHandle(), coord);
    }
    
    // Hide cursor
    inline void HideCursor() {
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(GetConsoleHandle(), &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(GetConsoleHandle(), &cursorInfo);
    }
    
    // Print unified control panel with status (single clean display)
    inline void PrintUnifiedPanel(int frame, int players, int enemies, int teammates,
                                   bool aimbot, bool triggerbot, bool bhop, bool boneESP,
                                   bool snaplines, bool distance, bool wallCheck, bool teamCheck, bool chams, bool inGame = false) {
        SetCursorPosition(0, 0);
        
        // Subaro ASCII Logo - Blue in lobby, Red in-game
        SetColor(inGame ? RED : CYAN);
        std::cout << "   _____       __                     \n";
        std::cout << "  / ___/__  __/ /_  ____ __________  \n";
        std::cout << "  \\__ \\/ / / / __ \\/ __ `/ ___/ __ \\ \n";
        std::cout << " ___/ / /_/ / /_/ / /_/ / /  / /_/ / \n";
        std::cout << "/____/\\__,_/_.___/\\__,_/_/   \\____/  \n";
        Reset();
        SetColor(DARK_GRAY);
        std::cout << "                          By: zrorisc\n";
        Reset();
        
        // Header - Blue in lobby, Red in-game
        SetColor(inGame ? RED : CYAN);
        std::cout << "========================================\n";
        std::cout << "           SUBARO - CS2 TOOL            \n";
        std::cout << "========================================\n";
        Reset();
        
        // Game Info
        std::cout << "Frame: ";
        SetColor(DARK_GRAY);
        std::cout << std::setw(6) << frame;
        Reset();
        std::cout << " | Players: ";
        SetColor(WHITE);
        std::cout << std::setw(2) << players;
        Reset();
        std::cout << " (";
        SetColor(RED);
        std::cout << enemies << " enemies";
        Reset();
        std::cout << ", ";
        SetColor(GREEN);
        std::cout << teammates << " team";
        Reset();
        std::cout << ")\n";
        
        SetColor(DARK_GRAY);
        std::cout << "----------------------------------------\n";
        Reset();
        
        // Features with keybinds
        std::cout << "[0] Debug Console    ";
        SetColor(DARK_GRAY);
        std::cout << "OFF\n";
        Reset();
        
        std::cout << "[1] Aimbot           ";
        SetColor(aimbot ? GREEN : DARK_GRAY);
        std::cout << (aimbot ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[2] Triggerbot       ";
        SetColor(triggerbot ? GREEN : DARK_GRAY);
        std::cout << (triggerbot ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[3] BunnyHop         ";
        SetColor(bhop ? GREEN : DARK_GRAY);
        std::cout << (bhop ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[4] Head Line        ";
        SetColor(boneESP ? GREEN : DARK_GRAY);
        std::cout << (boneESP ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[5] Snaplines        ";
        SetColor(snaplines ? GREEN : DARK_GRAY);
        std::cout << (snaplines ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[6] Distance ESP     ";
        SetColor(distance ? GREEN : DARK_GRAY);
        std::cout << (distance ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[7] Wall-Check       ";
        SetColor(wallCheck ? GREEN : DARK_GRAY);
        std::cout << (wallCheck ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[8] Team Check       ";
        SetColor(teamCheck ? GREEN : DARK_GRAY);
        std::cout << (teamCheck ? "ON " : "OFF") << "\n";
        Reset();
        
        std::cout << "[9] Chams            ";
        SetColor(chams ? GREEN : DARK_GRAY);
        std::cout << (chams ? "ON " : "OFF") << "\n";
        Reset();
        
        SetColor(CYAN);
        std::cout << "========================================\n";
        Reset();
        
        std::cout.flush();
    }
}
