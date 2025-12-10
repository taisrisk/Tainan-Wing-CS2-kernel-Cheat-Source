#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "offset_updater.hpp"
#include "header_parser.hpp"

// Simple JSON value extractor (no library needed)
namespace SimpleJSON {
    inline std::string extractString(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";

        size_t colonPos = json.find(':', pos);
        if (colonPos == std::string::npos) return "";

        size_t valueStart = json.find_first_not_of(" \t\n\r\"", colonPos + 1);
        if (valueStart == std::string::npos) return "";

        size_t valueEnd = json.find_first_of(",}\"", valueStart);
        if (valueEnd == std::string::npos) return "";

        return json.substr(valueStart, valueEnd - valueStart);
    }

    inline uint32_t extractHex(const std::string& json, const std::string& key) {
        std::string value = extractString(json, key);
        if (value.empty()) return 0;

        try {
            return static_cast<uint32_t>(std::stoul(value, nullptr, 0));
        }
        catch (...) {
            return 0;
        }
    }
}

// Dynamic CS2 Offsets Structure (Nova-style)
struct CS2Offsets {
    // Global offsets from offsets.json
    uint32_t dwEntityList;
    uint32_t dwViewMatrix;
    uint32_t dwViewAngles;
    uint32_t dwLocalPlayerPawn;
    uint32_t dwLocalPlayerController;
    uint32_t dwGlobalVars;
    uint32_t dwPlantedC4;

    // Buttons from buttons.json
    struct {
        uint32_t attack;
        uint32_t jump;
        uint32_t right;
        uint32_t left;
    } buttons;

    // C_BaseEntity offsets from client_dll.json
    uint32_t m_iHealth;
    uint32_t m_iMaxHealth;
    uint32_t m_iTeamNum;
    uint32_t m_fFlags;
    uint32_t m_vecAbsVelocity;
    uint32_t m_pGameSceneNode;
    uint32_t m_lifeState;

    // C_CSPlayerPawn offsets
    uint32_t m_hPlayerPawn;
    uint32_t m_angEyeAngles;
    uint32_t m_vecLastClipCameraPos;
    uint32_t m_aimPunchAngle;
    uint32_t m_iShotsFired;
    uint32_t m_pCameraServices;
    uint32_t m_pBulletServices;
    uint32_t m_pClippingWeapon;
    uint32_t m_iIDEntIndex;
    uint32_t m_bIsScoped;
    uint32_t m_bIsDefusing;
    uint32_t m_flFlashDuration;

    // CGameSceneNode offsets
    uint32_t m_vecAbsOrigin;
    uint32_t m_modelState;

    // CCSPlayerBase_CameraServices
    uint32_t m_iFOVStart;

    // C_BasePlayerController
    uint32_t m_iszPlayerName;

    // NEW: Crouch detection - proper offset chain
    uint32_t m_pMovementServices;  // Pointer to movement services
    uint32_t m_bInCrouch;          // Bool indicating if player is in crouch state
    uint32_t m_bDucked;            // Legacy fallback
    
    // NEW: Chams/Glow offsets
    uint32_t m_clrRender;          // Model render color (RGBA)
    uint32_t m_bGlowEnabled;       // Enable/disable glow
    uint32_t m_glowColorOverride;  // Glow color override
    
    // NEW: Bomb timer and planted C4 info
    uint32_t m_bBombTicking;
    uint32_t m_flC4Blow;
    uint32_t m_nBombSite;
    uint32_t m_bBombPlanted;
    uint32_t m_bBombDefused;
    uint32_t m_flTimerLength;
    uint32_t m_bBeingDefused;
    uint32_t m_flDefuseCountDown;
    uint32_t m_bBombDropped;

    CS2Offsets() {
        // Zero-initialize all fields
        dwEntityList = 0;
        dwViewMatrix = 0;
        dwViewAngles = 0;
        dwLocalPlayerPawn = 0;
        dwLocalPlayerController = 0;
        dwGlobalVars = 0;
        dwPlantedC4 = 0;
        buttons.attack = 0;
        buttons.jump = 0;
        buttons.right = 0;
        buttons.left = 0;
        m_iHealth = 0;
        m_iMaxHealth = 0;
        m_iTeamNum = 0;
        m_fFlags = 0;
        m_vecAbsVelocity = 0;
        m_pGameSceneNode = 0;
        m_lifeState = 0;
        m_hPlayerPawn = 0;
        m_angEyeAngles = 0;
        m_vecLastClipCameraPos = 0;
        m_aimPunchAngle = 0;
        m_iShotsFired = 0;
        m_pCameraServices = 0;
        m_pBulletServices = 0;
        m_pClippingWeapon = 0;
        m_iIDEntIndex = 0;
        m_bIsScoped = 0;
        m_bIsDefusing = 0;
        m_flFlashDuration = 0;
        m_vecAbsOrigin = 0;
        m_modelState = 0;
        m_iFOVStart = 0;
        m_iszPlayerName = 0;
        m_pMovementServices = 0;
        m_bInCrouch = 0;
        m_bDucked = 0;
        m_clrRender = 0;
        m_bGlowEnabled = 0;
        m_glowColorOverride = 0;
        m_bBombTicking = 0;
    }
};

class OffsetsManager {
private:
    static CS2Offsets offsets;
    static bool initialized;
    static const std::string cache_file;
    static const std::string client_dll_hpp_cache;
    static const std::string offsets_hpp_cache;
    static const std::string buttons_hpp_cache;

    // Save offsets to cache file
    static void SaveToCache(const std::string& client_dll, const std::string& offsets_data, const std::string& buttons_data) {
        try {
            std::ofstream file(cache_file);
            if (file.is_open()) {
                file << "CLIENT_DLL_JSON_START\n" << client_dll << "\nCLIENT_DLL_JSON_END\n";
                file << "OFFSETS_JSON_START\n" << offsets_data << "\nOFFSETS_JSON_END\n";
                file << "BUTTONS_JSON_START\n" << buttons_data << "\nBUTTONS_JSON_END\n";
                file.close();
            }
        }
        catch (...) {
            std::cerr << "[-] Failed to save offset cache\n";
        }
    }

    // Load offsets from cache file
    static bool LoadFromCache(std::string& client_dll, std::string& offsets_data, std::string& buttons_data) {
        try {
            std::ifstream file(cache_file);
            if (!file.is_open()) {
                return false;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();

            // Extract sections
            size_t cdll_start = content.find("CLIENT_DLL_JSON_START\n");
            size_t cdll_end = content.find("\nCLIENT_DLL_JSON_END");
            size_t off_start = content.find("OFFSETS_JSON_START\n");
            size_t off_end = content.find("\nOFFSETS_JSON_END");
            size_t btn_start = content.find("BUTTONS_JSON_START\n");
            size_t btn_end = content.find("\nBUTTONS_JSON_END");

            if (cdll_start == std::string::npos || cdll_end == std::string::npos ||
                off_start == std::string::npos || off_end == std::string::npos ||
                btn_start == std::string::npos || btn_end == std::string::npos) {
                return false;
            }

            client_dll = content.substr(cdll_start + 22, cdll_end - (cdll_start + 22));
            offsets_data = content.substr(off_start + 19, off_end - (off_start + 19));
            buttons_data = content.substr(btn_start + 19, btn_end - (btn_start + 19));
            return true;
        }
        catch (...) {
            return false;
        }
    }

    // Parse offsets from downloaded JSON data using simple string extraction
    static bool ParseOffsets(const std::string& client_dll_json, const std::string& offsets_json, const std::string& buttons_json) {
        try {
            using namespace SimpleJSON;

            // Extract global offsets from offsets.json
            offsets.dwEntityList = extractHex(offsets_json, "dwEntityList");
            offsets.dwViewMatrix = extractHex(offsets_json, "dwViewMatrix");
            offsets.dwViewAngles = extractHex(offsets_json, "dwViewAngles");
            offsets.dwLocalPlayerPawn = extractHex(offsets_json, "dwLocalPlayerPawn");
            offsets.dwLocalPlayerController = extractHex(offsets_json, "dwLocalPlayerController");
            offsets.dwGlobalVars = extractHex(offsets_json, "dwGlobalVars");
            offsets.dwPlantedC4 = extractHex(offsets_json, "dwPlantedC4");

            // Extract buttons from buttons.json
            offsets.buttons.attack = extractHex(buttons_json, "attack");
            offsets.buttons.jump = extractHex(buttons_json, "jump");
            offsets.buttons.right = extractHex(buttons_json, "right");
            offsets.buttons.left = extractHex(buttons_json, "left");

            // Extract class field offsets from client_dll.json
            offsets.m_iHealth = extractHex(client_dll_json, "m_iHealth");
            offsets.m_iMaxHealth = extractHex(client_dll_json, "m_iMaxHealth");
            offsets.m_iTeamNum = extractHex(client_dll_json, "m_iTeamNum");
            offsets.m_fFlags = extractHex(client_dll_json, "m_fFlags");
            offsets.m_vecAbsVelocity = extractHex(client_dll_json, "m_vecAbsVelocity");
            offsets.m_pGameSceneNode = extractHex(client_dll_json, "m_pGameSceneNode");
            offsets.m_lifeState = extractHex(client_dll_json, "m_lifeState");
            offsets.m_hPlayerPawn = extractHex(client_dll_json, "m_hPlayerPawn");
            offsets.m_angEyeAngles = extractHex(client_dll_json, "m_angEyeAngles");
            offsets.m_vecLastClipCameraPos = extractHex(client_dll_json, "m_vecLastClipCameraPos");
            offsets.m_aimPunchAngle = extractHex(client_dll_json, "m_aimPunchAngle");
            offsets.m_iShotsFired = extractHex(client_dll_json, "m_iShotsFired");
            offsets.m_pCameraServices = extractHex(client_dll_json, "m_pCameraServices");
            offsets.m_pBulletServices = extractHex(client_dll_json, "m_pBulletServices");
            offsets.m_pClippingWeapon = extractHex(client_dll_json, "m_pClippingWeapon");
            offsets.m_iIDEntIndex = extractHex(client_dll_json, "m_iIDEntIndex");
            offsets.m_bIsScoped = extractHex(client_dll_json, "m_bIsScoped");
            offsets.m_bIsDefusing = extractHex(client_dll_json, "m_bIsDefusing");
            offsets.m_flFlashDuration = extractHex(client_dll_json, "m_flFlashDuration");
            offsets.m_vecAbsOrigin = extractHex(client_dll_json, "m_vecAbsOrigin");
            offsets.m_modelState = extractHex(client_dll_json, "m_modelState");
            offsets.m_iFOVStart = extractHex(client_dll_json, "m_iFOVStart");
            offsets.m_iszPlayerName = extractHex(client_dll_json, "m_iszPlayerName");
            offsets.m_pMovementServices = extractHex(client_dll_json, "m_pMovementServices");
            offsets.m_bInCrouch = extractHex(client_dll_json, "m_bInCrouch");
            offsets.m_bDucked = extractHex(client_dll_json, "m_bDucked");
            offsets.m_clrRender = extractHex(client_dll_json, "m_clrRender");
            offsets.m_bGlowEnabled = extractHex(client_dll_json, "m_bGlowEnabled");
            offsets.m_glowColorOverride = extractHex(client_dll_json, "m_glowColorOverride");

            // NEW: Parse bomb timer and planted C4 info from client_dll.json
            // These are found in CPlantedC4 class in client_dll.hpp
            offsets.m_bBombTicking = extractHex(client_dll_json, "m_bBombTicking");
            offsets.m_flC4Blow = extractHex(client_dll_json, "m_flC4Blow");
            offsets.m_nBombSite = extractHex(client_dll_json, "m_nBombSite");
            offsets.m_flTimerLength = extractHex(client_dll_json, "m_flTimerLength");
            offsets.m_bBeingDefused = extractHex(client_dll_json, "m_bBeingDefused");
            offsets.m_flDefuseCountDown = extractHex(client_dll_json, "m_flDefuseCountDown");
            offsets.m_bBombDefused = extractHex(client_dll_json, "m_bBombDefused");
            
            // These are from CSGameRules in client_dll.hpp
            offsets.m_bBombPlanted = extractHex(client_dll_json, "m_bBombPlanted");
            offsets.m_bBombDropped = extractHex(client_dll_json, "m_bBombDropped");

            // Fallback for missing values
            if (offsets.m_lifeState == 0) offsets.m_lifeState = 0x354;
            
            // Fallback for movement services and crouch detection
            // m_pMovementServices from C_BasePlayerPawn
            if (offsets.m_pMovementServices == 0) offsets.m_pMovementServices = 0x1138;
            // m_bInCrouch from CPlayer_MovementServices_Humanoid (relative to movement services)
            if (offsets.m_bInCrouch == 0) offsets.m_bInCrouch = 0x240;
            // m_bDucked as fallback
            if (offsets.m_bDucked == 0) offsets.m_bDucked = 0x24C;
            
            // Fallback for chams/glow offsets
            if (offsets.m_clrRender == 0) offsets.m_clrRender = 0x7C0;
            if (offsets.m_bGlowEnabled == 0) offsets.m_bGlowEnabled = 0x444;
            if (offsets.m_glowColorOverride == 0) offsets.m_glowColorOverride = 0x468;
            
            // Fallback for bomb timer offsets using hardcoded values from client_dll.hpp
            if (offsets.m_bBombTicking == 0) offsets.m_bBombTicking = 0xED0;
            if (offsets.m_flC4Blow == 0) offsets.m_flC4Blow = 0xF00;
            if (offsets.m_nBombSite == 0) offsets.m_nBombSite = 0xED4;
            if (offsets.m_flTimerLength == 0) offsets.m_flTimerLength = 0xF08;
            if (offsets.m_bBeingDefused == 0) offsets.m_bBeingDefused = 0xF0C;
            if (offsets.m_flDefuseCountDown == 0) offsets.m_flDefuseCountDown = 0xF20;
            if (offsets.m_bBombDefused == 0) offsets.m_bBombDefused = 0xF24;
            if (offsets.m_bBombPlanted == 0) offsets.m_bBombPlanted = 0x9A5;
            if (offsets.m_bBombDropped == 0) offsets.m_bBombDropped = 0x9A4;

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "[-] Failed to parse offsets: " << e.what() << "\n";
            return false;
        }
    }

    // NEW: Parse offsets from C++ header files (offsets.hpp, buttons.hpp, client_dll.hpp)
    static bool ParseOffsetsFromHeaders(const std::string& offsets_hpp, const std::string& buttons_hpp, const std::string& client_dll_hpp) {
        try {
            std::cout << "[*] Parsing offsets from C++ header files...\n";
            
            // Parse each header file
            auto offsetsMap = HeaderParser::ParseOffsetsHpp(offsets_hpp);
            auto buttonsMap = HeaderParser::ParseButtonsHpp(buttons_hpp);
            auto clientDllMap = HeaderParser::ParseClientDllHpp(client_dll_hpp);
            
            // Debug: Print parsed counts
            std::cout << "[+] Parsed " << offsetsMap.size() << " global offsets\n";
            std::cout << "[+] Parsed " << buttonsMap.size() << " button offsets\n";
            std::cout << "[+] Parsed " << clientDllMap.size() << " class member offsets\n";
            
            // Extract global offsets from offsets.hpp
            offsets.dwEntityList = HeaderParser::GetOffset(offsetsMap, "dwEntityList");
            offsets.dwViewMatrix = HeaderParser::GetOffset(offsetsMap, "dwViewMatrix");
            offsets.dwViewAngles = HeaderParser::GetOffset(offsetsMap, "dwViewAngles");
            offsets.dwLocalPlayerPawn = HeaderParser::GetOffset(offsetsMap, "dwLocalPlayerPawn");
            offsets.dwLocalPlayerController = HeaderParser::GetOffset(offsetsMap, "dwLocalPlayerController");
            offsets.dwGlobalVars = HeaderParser::GetOffset(offsetsMap, "dwGlobalVars");
            offsets.dwPlantedC4 = HeaderParser::GetOffset(offsetsMap, "dwPlantedC4");
            
            // Extract buttons from buttons.hpp
            offsets.buttons.attack = HeaderParser::GetOffset(buttonsMap, "attack");
            offsets.buttons.jump = HeaderParser::GetOffset(buttonsMap, "jump");
            offsets.buttons.right = HeaderParser::GetOffset(buttonsMap, "right");
            offsets.buttons.left = HeaderParser::GetOffset(buttonsMap, "left");
            
            // Extract class member offsets from client_dll.hpp
            offsets.m_iHealth = HeaderParser::GetOffset(clientDllMap, "m_iHealth");
            offsets.m_iMaxHealth = HeaderParser::GetOffset(clientDllMap, "m_iMaxHealth");
            offsets.m_iTeamNum = HeaderParser::GetOffset(clientDllMap, "m_iTeamNum");
            offsets.m_fFlags = HeaderParser::GetOffset(clientDllMap, "m_fFlags");
            offsets.m_vecAbsVelocity = HeaderParser::GetOffset(clientDllMap, "m_vecAbsVelocity");
            offsets.m_pGameSceneNode = HeaderParser::GetOffset(clientDllMap, "m_pGameSceneNode");
            offsets.m_lifeState = HeaderParser::GetOffset(clientDllMap, "m_lifeState", 0x354);
            offsets.m_hPlayerPawn = HeaderParser::GetOffset(clientDllMap, "m_hPlayerPawn");
            offsets.m_angEyeAngles = HeaderParser::GetOffset(clientDllMap, "m_angEyeAngles");
            offsets.m_vecLastClipCameraPos = HeaderParser::GetOffset(clientDllMap, "m_vecLastClipCameraPos");
            offsets.m_aimPunchAngle = HeaderParser::GetOffset(clientDllMap, "m_aimPunchAngle");
            offsets.m_iShotsFired = HeaderParser::GetOffset(clientDllMap, "m_iShotsFired");
            offsets.m_pCameraServices = HeaderParser::GetOffset(clientDllMap, "m_pCameraServices");
            offsets.m_pBulletServices = HeaderParser::GetOffset(clientDllMap, "m_pBulletServices");
            offsets.m_pClippingWeapon = HeaderParser::GetOffset(clientDllMap, "m_pClippingWeapon");
            offsets.m_iIDEntIndex = HeaderParser::GetOffset(clientDllMap, "m_iIDEntIndex");
            offsets.m_bIsScoped = HeaderParser::GetOffset(clientDllMap, "m_bIsScoped");
            offsets.m_bIsDefusing = HeaderParser::GetOffset(clientDllMap, "m_bIsDefusing");
            offsets.m_flFlashDuration = HeaderParser::GetOffset(clientDllMap, "m_flFlashDuration");
            offsets.m_vecAbsOrigin = HeaderParser::GetOffset(clientDllMap, "m_vecAbsOrigin");
            offsets.m_modelState = HeaderParser::GetOffset(clientDllMap, "m_modelState");
            offsets.m_iFOVStart = HeaderParser::GetOffset(clientDllMap, "m_iFOVStart");
            offsets.m_iszPlayerName = HeaderParser::GetOffset(clientDllMap, "m_iszPlayerName");
            
            // Crouch detection offsets
            offsets.m_pMovementServices = HeaderParser::GetOffset(clientDllMap, "m_pMovementServices", 0x1138);
            offsets.m_bInCrouch = HeaderParser::GetOffset(clientDllMap, "m_bInCrouch", 0x240);
            offsets.m_bDucked = HeaderParser::GetOffset(clientDllMap, "m_bDucked", 0x24C);
            
            // Chams/Glow offsets
            offsets.m_clrRender = HeaderParser::GetOffset(clientDllMap, "m_clrRender", 0x7C0);
            offsets.m_bGlowEnabled = HeaderParser::GetOffset(clientDllMap, "m_bGlowEnabled", 0x444);
            offsets.m_glowColorOverride = HeaderParser::GetOffset(clientDllMap, "m_glowColorOverride", 0x468);
            
            // Bomb timer offsets
            offsets.m_bBombTicking = HeaderParser::GetOffset(clientDllMap, "m_bBombTicking", 0xED0);
            offsets.m_flC4Blow = HeaderParser::GetOffset(clientDllMap, "m_flC4Blow", 0xF00);
            offsets.m_nBombSite = HeaderParser::GetOffset(clientDllMap, "m_nBombSite", 0xED4);
            offsets.m_flTimerLength = HeaderParser::GetOffset(clientDllMap, "m_flTimerLength", 0xF08);
            offsets.m_bBeingDefused = HeaderParser::GetOffset(clientDllMap, "m_bBeingDefused", 0xF0C);
            offsets.m_flDefuseCountDown = HeaderParser::GetOffset(clientDllMap, "m_flDefuseCountDown", 0xF20);
            offsets.m_bBombDefused = HeaderParser::GetOffset(clientDllMap, "m_bBombDefused", 0xF24);
            offsets.m_bBombPlanted = HeaderParser::GetOffset(clientDllMap, "m_bBombPlanted", 0x9A5);
            offsets.m_bBombDropped = HeaderParser::GetOffset(clientDllMap, "m_bBombDropped", 0x9A4);
            
            std::cout << "[+] Successfully parsed offsets from header files\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "[-] Failed to parse offsets from headers: " << e.what() << "\n";
            return false;
        }
    }

    // Load header files from disk
    static bool LoadHeaderFiles(std::string& offsets_hpp, std::string& buttons_hpp, std::string& client_dll_hpp) {
        try {
            // Try to load offsets.hpp
            std::ifstream offsets_file(offsets_hpp_cache);
            if (!offsets_file.is_open()) return false;
            std::stringstream offsets_buffer;
            offsets_buffer << offsets_file.rdbuf();
            offsets_hpp = offsets_buffer.str();
            offsets_file.close();
            
            // Try to load buttons.hpp
            std::ifstream buttons_file(buttons_hpp_cache);
            if (!buttons_file.is_open()) return false;
            std::stringstream buttons_buffer;
            buttons_buffer << buttons_file.rdbuf();
            buttons_hpp = buttons_buffer.str();
            buttons_file.close();
            
            // Try to load client_dll.hpp
            std::ifstream client_dll_file(client_dll_hpp_cache);
            if (!client_dll_file.is_open()) return false;
            std::stringstream client_dll_buffer;
            client_dll_buffer << client_dll_file.rdbuf();
            client_dll_hpp = client_dll_buffer.str();
            client_dll_file.close();
            
            return !offsets_hpp.empty() && !buttons_hpp.empty() && !client_dll_hpp.empty();
        }
        catch (...) {
            return false;
        }
    }

    // Download C++ header files if they don't exist
    static void DownloadHeaderFilesIfNeeded() {
        // Check if all header files exist
        std::ifstream check_offsets(offsets_hpp_cache);
        std::ifstream check_buttons(buttons_hpp_cache);
        std::ifstream check_client(client_dll_hpp_cache);
        
        bool all_exist = check_offsets.good() && check_buttons.good() && check_client.good();
        check_offsets.close();
        check_buttons.close();
        check_client.close();
        
        if (all_exist) {
            std::cout << "[+] All C++ header files already exist\n";
            return;
        }
        
        std::cout << "[*] Downloading C++ header files from a2x/cs2-dumper...\n";
        
        // Download offsets.hpp
        std::string offsets_hpp;
        if (OffsetUpdater::DownloadOffsetsHpp(offsets_hpp)) {
            std::ofstream out(offsets_hpp_cache);
            if (out.is_open()) {
                out << offsets_hpp;
                out.close();
                std::cout << "[+] offsets.hpp downloaded\n";
            }
        }
        
        // Download buttons.hpp
        std::string buttons_hpp;
        if (OffsetUpdater::DownloadButtonsHpp(buttons_hpp)) {
            std::ofstream out(buttons_hpp_cache);
            if (out.is_open()) {
                out << buttons_hpp;
                out.close();
                std::cout << "[+] buttons.hpp downloaded\n";
            }
        }
        
        // Download client_dll.hpp
        std::string client_dll_hpp;
        if (OffsetUpdater::DownloadClientDllHpp(client_dll_hpp)) {
            std::ofstream out(client_dll_hpp_cache);
            if (out.is_open()) {
                out << client_dll_hpp;
                out.close();
                std::cout << "[+] client_dll.hpp downloaded\n";
            }
        }
    }

    // Download client_dll.hpp if it doesn't exist
    static void DownloadClientDllHppIfNeeded() {
        std::ifstream check(client_dll_hpp_cache);
        if (check.good()) {
            check.close();
            std::cout << "[+] client_dll.hpp already exists\n";
            return;
        }
        
        std::cout << "[*] Downloading client_dll.hpp from sezzyaep...\n";
        std::string client_dll_hpp;
        
        if (OffsetUpdater::DownloadClientDllHpp(client_dll_hpp)) {
            std::ofstream out(client_dll_hpp_cache);
            if (out.is_open()) {
                out << client_dll_hpp;
                out.close();
                std::cout << "[+] client_dll.hpp downloaded and saved\n";
            } else {
                std::cerr << "[-] Failed to save client_dll.hpp\n";
            }
        } else {
            std::cerr << "[-] Failed to download client_dll.hpp\n";
        }
    }

public:
    // Initialize the offset manager (call this once at startup)
    static bool Initialize() {
        if (initialized) {
            return true;
        }

        std::cout << "[*] Initializing CS2 Offsets Manager (local headers only)...\n";

        // Always load from local C++ header files in src/Offsets
        std::string offsets_hpp, buttons_hpp, client_dll_hpp;
        if (LoadHeaderFiles(offsets_hpp, buttons_hpp, client_dll_hpp)) {
            std::cout << "[+] Loaded offsets from local C++ header files\n";
            if (ParseOffsetsFromHeaders(offsets_hpp, buttons_hpp, client_dll_hpp)) {
                initialized = true;
                std::cout << "[+] Offsets initialized successfully from local C++ headers\n";
                return true;
            } else {
                std::cerr << "[-] Failed to parse offsets from local headers\n";
                return false;
            }
        }

        // No network or cache fallback by design
        std::cerr << "[-] Could not load local header files from src/Offsets. Ensure files exist.\n";
        return false;
    }

    // Get the current offsets
    static const CS2Offsets& Get() {
        if (!initialized) {
            throw std::runtime_error("OffsetsManager not initialized! Call Initialize() first.");
        }
        return offsets;
    }

    // Force update: re-read local headers (no network)
    static bool ForceUpdate() {
        std::cout << "[*] Force reloading offsets from local headers...\n";
        std::string offsets_hpp, buttons_hpp, client_dll_hpp;
        if (!LoadHeaderFiles(offsets_hpp, buttons_hpp, client_dll_hpp)) {
            std::cerr << "[-] Failed to read local header files\n";
            return false;
        }
        if (!ParseOffsetsFromHeaders(offsets_hpp, buttons_hpp, client_dll_hpp)) {
            std::cerr << "[-] Failed to parse local header files\n";
            return false;
        }
        initialized = true;
        std::cout << "[+] Offsets reloaded from local headers\n";
        return true;
    }

    // Print current offsets for debugging
    static void PrintOffsets() {
        if (!initialized) {
            std::cerr << "[-] Offsets not initialized\n";
            return;
        }

        std::cout << "\n===== CS2 OFFSETS =====\n";
        std::cout << "dwEntityList: 0x" << std::hex << offsets.dwEntityList << std::dec << "\n";
        std::cout << "dwViewMatrix: 0x" << std::hex << offsets.dwViewMatrix << std::dec << "\n";
        std::cout << "dwViewAngles: 0x" << std::hex << offsets.dwViewAngles << std::dec << "\n";
        std::cout << "dwLocalPlayerPawn: 0x" << std::hex << offsets.dwLocalPlayerPawn << std::dec << "\n";
        std::cout << "m_iHealth: 0x" << std::hex << offsets.m_iHealth << std::dec << "\n";
        std::cout << "m_iTeamNum: 0x" << std::hex << offsets.m_iTeamNum << std::dec << "\n";
        std::cout << "m_vecAbsOrigin: 0x" << std::hex << offsets.m_vecAbsOrigin << std::dec << "\n";
        std::cout << "m_hPlayerPawn: 0x" << std::hex << offsets.m_hPlayerPawn << std::dec << "\n";
        std::cout << "\n=== CROUCH DETECTION OFFSETS ===\n";
        std::cout << "m_pMovementServices: 0x" << std::hex << offsets.m_pMovementServices << std::dec << "\n";
        std::cout << "m_bInCrouch: 0x" << std::hex << offsets.m_bInCrouch << std::dec << "\n";
        std::cout << "m_bDucked (fallback): 0x" << std::hex << offsets.m_bDucked << std::dec << "\n";
        std::cout << "m_iszPlayerName: 0x" << std::hex << offsets.m_iszPlayerName << std::dec << "\n";
        std::cout << "\n=== CHAMS/GLOW OFFSETS ===\n";
        std::cout << "m_clrRender: 0x" << std::hex << offsets.m_clrRender << std::dec << "\n";
        std::cout << "m_bGlowEnabled: 0x" << std::hex << offsets.m_bGlowEnabled << std::dec << "\n";
        std::cout << "m_glowColorOverride: 0x" << std::hex << offsets.m_glowColorOverride << std::dec << "\n";
        std::cout << "\n=== BOMB TIMER OFFSETS ===\n";
        std::cout << "m_bBombTicking: 0x" << std::hex << offsets.m_bBombTicking << std::dec << "\n";
        std::cout << "m_flC4Blow: 0x" << std::hex << offsets.m_flC4Blow << std::dec << "\n";
        std::cout << "m_nBombSite: 0x" << std::hex << offsets.m_nBombSite << std::dec << "\n";
        std::cout << "m_bBombPlanted: 0x" << std::hex << offsets.m_bBombPlanted << std::dec << "\n";
        std::cout << "m_bBombDefused: 0x" << std::hex << offsets.m_bBombDefused << std::dec << "\n";
        std::cout << "m_flTimerLength: 0x" << std::hex << offsets.m_flTimerLength << std::dec << "\n";
        std::cout << "m_bBeingDefused: 0x" << std::hex << offsets.m_bBeingDefused << std::dec << "\n";
        std::cout << "m_flDefuseCountDown: 0x" << std::hex << offsets.m_flDefuseCountDown << std::dec << "\n";
        std::cout << "m_bBombDropped: 0x" << std::hex << offsets.m_bBombDropped << std::dec << "\n";
        std::cout << "=========================\n\n";
    }
};