#pragma once

#include <string>
#include <unordered_map>
#include <regex>
#include <sstream>
#include <iostream>

namespace HeaderParser {
    
    // Parse a single constexpr line like: "constexpr std::ptrdiff_t dwEntityList = 0x1D13CE8;"
    inline uint32_t ExtractOffsetFromLine(const std::string& line) {
        // Look for pattern: constexpr std::ptrdiff_t NAME = 0xHEXVALUE;
        std::regex pattern(R"(constexpr\s+std::ptrdiff_t\s+\w+\s*=\s*(0x[0-9A-Fa-f]+))");
        std::smatch match;
        
        if (std::regex_search(line, match, pattern)) {
            if (match.size() >= 2) {
                std::string hexValue = match[1].str();
                try {
                    return static_cast<uint32_t>(std::stoul(hexValue, nullptr, 16));
                } catch (...) {
                    return 0;
                }
            }
        }
        return 0;
    }
    
    // Extract offset name from line
    inline std::string ExtractOffsetName(const std::string& line) {
        std::regex pattern(R"(constexpr\s+std::ptrdiff_t\s+(\w+)\s*=)");
        std::smatch match;
        
        if (std::regex_search(line, match, pattern)) {
            if (match.size() >= 2) {
                return match[1].str();
            }
        }
        return "";
    }
    
    // Parse offsets.hpp (global offsets like dwEntityList, dwViewMatrix, etc)
    inline std::unordered_map<std::string, uint32_t> ParseOffsetsHpp(const std::string& content) {
        std::unordered_map<std::string, uint32_t> offsets;
        std::istringstream stream(content);
        std::string line;
        bool inClientDll = false;
        
        while (std::getline(stream, line)) {
            // Check if we're in the client_dll namespace
            if (line.find("namespace client_dll") != std::string::npos) {
                inClientDll = true;
                continue;
            }
            
            // Exit namespace when we hit a closing brace at column 0
            if (inClientDll && line.find("}") == 0) {
                inClientDll = false;
                continue;
            }
            
            // Only parse lines inside client_dll namespace
            if (inClientDll && line.find("constexpr std::ptrdiff_t") != std::string::npos) {
                std::string name = ExtractOffsetName(line);
                uint32_t value = ExtractOffsetFromLine(line);
                
                if (!name.empty() && value != 0) {
                    offsets[name] = value;
                }
            }
        }
        
        return offsets;
    }
    
    // Parse buttons.hpp (button offsets like attack, jump, etc)
    inline std::unordered_map<std::string, uint32_t> ParseButtonsHpp(const std::string& content) {
        std::unordered_map<std::string, uint32_t> buttons;
        std::istringstream stream(content);
        std::string line;
        bool inButtons = false;
        
        while (std::getline(stream, line)) {
            // Check if we're in the buttons namespace
            if (line.find("namespace buttons") != std::string::npos) {
                inButtons = true;
                continue;
            }
            
            // Exit namespace when we hit a closing brace at column 0
            if (inButtons && line.find("}") == 0) {
                inButtons = false;
                continue;
            }
            
            // Only parse lines inside buttons namespace
            if (inButtons && line.find("constexpr std::ptrdiff_t") != std::string::npos) {
                std::string name = ExtractOffsetName(line);
                uint32_t value = ExtractOffsetFromLine(line);
                
                if (!name.empty() && value != 0) {
                    buttons[name] = value;
                }
            }
        }
        
        return buttons;
    }
    
    // Parse client_dll.hpp (class member offsets like m_iHealth, m_iTeamNum, etc)
    // This file has a different structure with nested namespaces for each class
    inline std::unordered_map<std::string, uint32_t> ParseClientDllHpp(const std::string& content) {
        std::unordered_map<std::string, uint32_t> classOffsets;
        std::istringstream stream(content);
        std::string line;
        bool inClientDll = false;
        
        while (std::getline(stream, line)) {
            // Check if we're in the client_dll namespace
            if (line.find("namespace client_dll") != std::string::npos) {
                inClientDll = true;
                continue;
            }
            
            // Exit main namespace
            if (inClientDll && line.find("} // namespace client_dll") != std::string::npos) {
                inClientDll = false;
                continue;
            }
            
            // Parse any constexpr line inside client_dll
            if (inClientDll && line.find("constexpr std::ptrdiff_t") != std::string::npos) {
                std::string name = ExtractOffsetName(line);
                uint32_t value = ExtractOffsetFromLine(line);
                
                if (!name.empty() && value != 0) {
                    classOffsets[name] = value;
                }
            }
        }
        
        return classOffsets;
    }
    
    // Helper: Get offset by name with fallback
    inline uint32_t GetOffset(const std::unordered_map<std::string, uint32_t>& map, 
                              const std::string& key, 
                              uint32_t fallback = 0) {
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }
        return fallback;
    }
    
    // Debug: Print all parsed offsets
    inline void PrintOffsets(const std::string& title, const std::unordered_map<std::string, uint32_t>& offsets) {
        std::cout << "\n===== " << title << " =====\n";
        for (const auto& [name, value] : offsets) {
            std::cout << name << ": 0x" << std::hex << value << std::dec << "\n";
        }
        std::cout << "Total: " << offsets.size() << " offsets\n";
    }
}
