#pragma once

#include <string>
#include <windows.h>
#include <wininet.h>
#include <stdexcept>

#pragma comment(lib, "wininet.lib")

namespace OffsetUpdater {
    
    // Downloads a file from a URL using WinINet (similar to Nova's approach)
    inline std::string DownloadFile(const std::string& url) {
        HINTERNET hInternet = InternetOpenA(
            "CS2Cheat/1.0", 
            INTERNET_OPEN_TYPE_DIRECT, 
            nullptr, 
            nullptr, 
            0
        );
        
        if (!hInternet) {
            throw std::runtime_error("Failed to initialize WinINet");
        }
        
        HINTERNET hUrl = InternetOpenUrlA(
            hInternet, 
            url.c_str(), 
            nullptr, 
            0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE,
            0
        );
        
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            throw std::runtime_error("Failed to open URL: " + url);
        }
        
        std::string result;
        char buffer[8192];
        DWORD bytesRead = 0;
        
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            result.append(buffer, bytesRead);
        }
        
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        
        return result;
    }
    
    // Download all required offset files from cs2-dumper
    inline bool UpdateOffsetsFromGitHub(std::string& client_dll_json, std::string& offsets_json, std::string& buttons_json) {
        try {
            // Download from a2x/cs2-dumper main branch
            client_dll_json = DownloadFile(
                "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/client_dll.json"
            );
            
            offsets_json = DownloadFile(
                "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json"
            );
            
            buttons_json = DownloadFile(
                "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/buttons.json"
            );
            
            // Verify we got valid JSON (basic check)
            if (client_dll_json.empty() || offsets_json.empty() || buttons_json.empty()) {
                return false;
            }
            
            if (client_dll_json.find("{") == std::string::npos ||
                offsets_json.find("{") == std::string::npos ||
                buttons_json.find("{") == std::string::npos) {
                return false;
            }
            
            return true;
            
        } catch (...) {
            return false;
        }
    }

    // Download sezzyaep's client.dll.hpp with all the missing offsets
    inline bool DownloadClientDllHpp(std::string& client_dll_hpp) {
        try {
            client_dll_hpp = DownloadFile(
                "https://raw.githubusercontent.com/sezzyaep/CS2-OFFSETS/refs/heads/main/client.dll.hpp"
            );
            
            if (client_dll_hpp.empty() || client_dll_hpp.find("namespace") == std::string::npos) {
                return false;
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }
}
