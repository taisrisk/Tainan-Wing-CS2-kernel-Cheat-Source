#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "offset_updater.hpp"
#include "offset_decryptor.hpp"

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
    uint32_t dwCSGOInput;
    uint32_t dwInputSystem;
    uint32_t dwSensitivity;
    uint32_t dwSensitivity_sensitivity;

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
        dwCSGOInput = 0;
        dwInputSystem = 0;
        dwSensitivity = 0;
        dwSensitivity_sensitivity = 0;
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
        m_flC4Blow = 0;
        m_nBombSite = 0;
        m_bBombPlanted = 0;
        m_bBombDefused = 0;
        m_flTimerLength = 0;
        m_bBeingDefused = 0;
        m_flDefuseCountDown = 0;
        m_bBombDropped = 0;
    }
};

class OffsetsManager {
private:
    static CS2Offsets offsets;
    static bool initialized;
    static const std::string cache_file;
    static const std::string client_dll_hpp_cache;
    static const std::string header_cache_file;

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
            offsets.dwCSGOInput = extractHex(offsets_json, "dwCSGOInput");
            offsets.dwInputSystem = extractHex(offsets_json, "dwInputSystem");
            offsets.dwSensitivity = extractHex(offsets_json, "dwSensitivity");
            offsets.dwSensitivity_sensitivity = extractHex(offsets_json, "dwSensitivity_sensitivity");

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

    // Try loading JSON directly from local repo folder: src/Offsets/*.json (Nova-style)
    static bool InitializeFromLocalJsonImpl() {
        if (initialized) return true;
        auto try_read = [](const std::vector<std::string>& roots, const std::string& rel, std::string& out) -> bool {
            for (const auto& r : roots) {
                std::ifstream f(r + rel, std::ios::in | std::ios::binary);
                if (f.good()) {
                    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
                }
            }
            return false;
        };

        // Build search roots relative to current working dir
        std::vector<std::string> roots = {
            "",
            ".\\",
            "..\\",
            "..\\..\\",
            "..\\..\\..\\"
        };

        std::string client_dll_json, offsets_json, buttons_json;
        bool okOff = try_read(roots, std::string("src\\Offsets\\offsets.json"), offsets_json) ||
                     try_read(roots, std::string("Offsets\\offsets.json"), offsets_json);
        bool okBtn = try_read(roots, std::string("src\\Offsets\\buttons.json"), buttons_json) ||
                     try_read(roots, std::string("Offsets\\buttons.json"), buttons_json);
        bool okCdll = try_read(roots, std::string("src\\Offsets\\client_dll.json"), client_dll_json) ||
                      try_read(roots, std::string("Offsets\\client_dll.json"), client_dll_json);

        if (!(okOff && okBtn && okCdll)) {
            std::cerr << "[-] Local JSON offsets not found in src/Offsets" << "\n";
            return false;
        }

        if (!ParseOffsets(client_dll_json, offsets_json, buttons_json)) {
            std::cerr << "[-] Failed to parse local JSON offsets" << "\n";
            return false;
        }

        initialized = true;
        std::cout << "[+] Offsets initialized from local JSON (src/Offsets)" << "\n";
        return true;
    }

public:
    // Public wrapper to initialize from local JSON
    static bool InitializeFromLocalJson() {
        return InitializeFromLocalJsonImpl();
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

        std::cout << "[*] Initializing CS2 Offsets Manager...\n";

        // Try to load from cache first
        std::string client_dll_json, offsets_json, buttons_json;

        if (LoadFromCache(client_dll_json, offsets_json, buttons_json)) {
            std::cout << "[+] Loaded offsets from cache\n";
            if (ParseOffsets(client_dll_json, offsets_json, buttons_json)) {
                initialized = true;
                
                // Download client_dll.hpp if not exists
                DownloadClientDllHppIfNeeded();
                
                std::cout << "[+] Offsets initialized successfully from cache\n";
                return true;
            }
        }

        // Cache failed, try to download fresh offsets
        std::cout << "[*] Downloading latest offsets from GitHub...\n";

        if (!OffsetUpdater::UpdateOffsetsFromGitHub(client_dll_json, offsets_json, buttons_json)) {
            std::cerr << "[-] Failed to download offsets from GitHub\n";
            return false;
        }

        std::cout << "[+] Downloaded offsets successfully\n";

        // Parse downloaded offsets
        if (!ParseOffsets(client_dll_json, offsets_json, buttons_json)) {
            std::cerr << "[-] Failed to parse downloaded offsets\n";
            return false;
        }

        // Save to cache for next time
        SaveToCache(client_dll_json, offsets_json, buttons_json);
        
        // Download client_dll.hpp
        DownloadClientDllHppIfNeeded();

        initialized = true;
        std::cout << "[+] Offsets initialized successfully and cached\n";
        return true;
    }

    // Initialize by parsing local generated headers instead of JSON
    static bool InitializeFromHeaders() {
        if (initialized) return true;
        std::string log;
        CS2Offsets parsed; // zero-initialized by ctor
        if (!OffsetDecryptor::DecryptHeadersTo(parsed, &log)) {
            std::cerr << log;
            std::cerr << "[-] Header decryptor failed.\n";
            return false;
        }
        std::cout << log;
        offsets = parsed;
        initialized = true;
        std::cout << "[+] Offsets initialized from headers (offset_decryptor)\n";
        return true;
    }

    // Version-aware cached initialize from headers
    static bool InitializeFromHeadersCached(const std::string& appVersion) {
        if (initialized) return true;
        // Try load cached header parse first if version matches
        CS2Offsets cached{};
        std::string cachedVersion;
        if (LoadHeaderCache(cached, cachedVersion)) {
            if (!appVersion.empty() && cachedVersion == appVersion) {
                offsets = cached;
                initialized = true;
                std::cout << "[+] Loaded offsets from header cache (version match)\n";
                return true;
            }
        }

        // Parse from headers now
        std::string log;
        CS2Offsets parsed{};
        if (!OffsetDecryptor::DecryptHeadersTo(parsed, &log)) {
            std::cerr << log;
            std::cerr << "[-] Header decryptor failed.\n";
            return false;
        }
        std::cout << log;
        offsets = parsed;
        initialized = true;
        SaveHeaderCache(appVersion, offsets);
        std::cout << "[+] Offsets initialized from headers and cached\n";
        return true;
    }

    // Get the current offsets
    static const CS2Offsets& Get() {
        if (!initialized) {
            throw std::runtime_error("OffsetsManager not initialized! Call Initialize() first.");
        }
        return offsets;
    }

    // Force update from GitHub (ignoring cache)
    static bool ForceUpdate() {
        std::cout << "[*] Force updating offsets from GitHub...\n";
        std::string client_dll_json, offsets_json, buttons_json;

        if (!OffsetUpdater::UpdateOffsetsFromGitHub(client_dll_json, offsets_json, buttons_json)) {
            std::cerr << "[-] Failed to download offsets\n";
            return false;
        }

        if (!ParseOffsets(client_dll_json, offsets_json, buttons_json)) {
            std::cerr << "[-] Failed to parse offsets\n";
            return false;
        }

        // Save to cache
        SaveToCache(client_dll_json, offsets_json, buttons_json);

        std::cout << "[+] Offsets force updated successfully\n";
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
private:
    static bool LoadHeaderCache(CS2Offsets& out, std::string& outVersion) {
        try {
            std::ifstream f(header_cache_file);
            if (!f.is_open()) return false;
            std::stringstream ss; ss << f.rdbuf();
            std::string content = ss.str();
            outVersion = SimpleJSON::extractString(content, "version");
            if (outVersion.empty()) return false;

            auto HX = [&](const char* k){ return SimpleJSON::extractHex(content, k); };

            out.dwEntityList = HX("dwEntityList");
            out.dwViewMatrix = HX("dwViewMatrix");
            out.dwViewAngles = HX("dwViewAngles");
            out.dwLocalPlayerPawn = HX("dwLocalPlayerPawn");
            out.dwLocalPlayerController = HX("dwLocalPlayerController");
            out.dwGlobalVars = HX("dwGlobalVars");
            out.dwPlantedC4 = HX("dwPlantedC4");
            out.dwCSGOInput = HX("dwCSGOInput");
            out.dwInputSystem = HX("dwInputSystem");
            out.dwSensitivity = HX("dwSensitivity");
            out.dwSensitivity_sensitivity = HX("dwSensitivity_sensitivity");

            out.buttons.attack = HX("buttons.attack");
            out.buttons.jump = HX("buttons.jump");
            out.buttons.right = HX("buttons.right");
            out.buttons.left = HX("buttons.left");

            out.m_iHealth = HX("m_iHealth");
            out.m_iMaxHealth = HX("m_iMaxHealth");
            out.m_iTeamNum = HX("m_iTeamNum");
            out.m_fFlags = HX("m_fFlags");
            out.m_vecAbsVelocity = HX("m_vecAbsVelocity");
            out.m_pGameSceneNode = HX("m_pGameSceneNode");
            out.m_lifeState = HX("m_lifeState");
            out.m_hPlayerPawn = HX("m_hPlayerPawn");
            out.m_angEyeAngles = HX("m_angEyeAngles");
            out.m_vecLastClipCameraPos = HX("m_vecLastClipCameraPos");
            out.m_aimPunchAngle = HX("m_aimPunchAngle");
            out.m_iShotsFired = HX("m_iShotsFired");
            out.m_pCameraServices = HX("m_pCameraServices");
            out.m_pBulletServices = HX("m_pBulletServices");
            out.m_pClippingWeapon = HX("m_pClippingWeapon");
            out.m_iIDEntIndex = HX("m_iIDEntIndex");
            out.m_bIsScoped = HX("m_bIsScoped");
            out.m_bIsDefusing = HX("m_bIsDefusing");
            out.m_flFlashDuration = HX("m_flFlashDuration");
            out.m_vecAbsOrigin = HX("m_vecAbsOrigin");
            out.m_modelState = HX("m_modelState");
            out.m_iFOVStart = HX("m_iFOVStart");
            out.m_iszPlayerName = HX("m_iszPlayerName");
            out.m_pMovementServices = HX("m_pMovementServices");
            out.m_bInCrouch = HX("m_bInCrouch");
            out.m_bDucked = HX("m_bDucked");
            out.m_clrRender = HX("m_clrRender");
            out.m_bGlowEnabled = HX("m_bGlowEnabled");
            out.m_glowColorOverride = HX("m_glowColorOverride");
            out.m_bBombTicking = HX("m_bBombTicking");
            out.m_flC4Blow = HX("m_flC4Blow");
            out.m_nBombSite = HX("m_nBombSite");
            out.m_flTimerLength = HX("m_flTimerLength");
            out.m_bBeingDefused = HX("m_bBeingDefused");
            out.m_flDefuseCountDown = HX("m_flDefuseCountDown");
            out.m_bBombDefused = HX("m_bBombDefused");
            out.m_bBombPlanted = HX("m_bBombPlanted");
            out.m_bBombDropped = HX("m_bBombDropped");
            return true;
        } catch (...) { return false; }
    }

    static void SaveHeaderCache(const std::string& version, const CS2Offsets& in) {
        try {
            std::ofstream f(header_cache_file, std::ios::trunc);
            if (!f.is_open()) return;
            auto H = [](uint32_t v){ std::ostringstream o; o << "0x" << std::uppercase << std::hex << v; return o.str(); };
            f << "{\n";
            f << "  \"version\": \"" << version << "\",\n";
            f << "  \"dwEntityList\": \"" << H(in.dwEntityList) << "\",\n";
            f << "  \"dwViewMatrix\": \"" << H(in.dwViewMatrix) << "\",\n";
            f << "  \"dwViewAngles\": \"" << H(in.dwViewAngles) << "\",\n";
            f << "  \"dwLocalPlayerPawn\": \"" << H(in.dwLocalPlayerPawn) << "\",\n";
            f << "  \"dwLocalPlayerController\": \"" << H(in.dwLocalPlayerController) << "\",\n";
            f << "  \"dwGlobalVars\": \"" << H(in.dwGlobalVars) << "\",\n";
            f << "  \"dwPlantedC4\": \"" << H(in.dwPlantedC4) << "\",\n";
            f << "  \"dwCSGOInput\": \"" << H(in.dwCSGOInput) << "\",\n";
            f << "  \"dwInputSystem\": \"" << H(in.dwInputSystem) << "\",\n";
            f << "  \"dwSensitivity\": \"" << H(in.dwSensitivity) << "\",\n";
            f << "  \"dwSensitivity_sensitivity\": \"" << H(in.dwSensitivity_sensitivity) << "\",\n";
            f << "  \"buttons.attack\": \"" << H(in.buttons.attack) << "\",\n";
            f << "  \"buttons.jump\": \"" << H(in.buttons.jump) << "\",\n";
            f << "  \"buttons.right\": \"" << H(in.buttons.right) << "\",\n";
            f << "  \"buttons.left\": \"" << H(in.buttons.left) << "\",\n";
            f << "  \"m_iHealth\": \"" << H(in.m_iHealth) << "\",\n";
            f << "  \"m_iMaxHealth\": \"" << H(in.m_iMaxHealth) << "\",\n";
            f << "  \"m_iTeamNum\": \"" << H(in.m_iTeamNum) << "\",\n";
            f << "  \"m_fFlags\": \"" << H(in.m_fFlags) << "\",\n";
            f << "  \"m_vecAbsVelocity\": \"" << H(in.m_vecAbsVelocity) << "\",\n";
            f << "  \"m_pGameSceneNode\": \"" << H(in.m_pGameSceneNode) << "\",\n";
            f << "  \"m_lifeState\": \"" << H(in.m_lifeState) << "\",\n";
            f << "  \"m_hPlayerPawn\": \"" << H(in.m_hPlayerPawn) << "\",\n";
            f << "  \"m_angEyeAngles\": \"" << H(in.m_angEyeAngles) << "\",\n";
            f << "  \"m_vecLastClipCameraPos\": \"" << H(in.m_vecLastClipCameraPos) << "\",\n";
            f << "  \"m_aimPunchAngle\": \"" << H(in.m_aimPunchAngle) << "\",\n";
            f << "  \"m_iShotsFired\": \"" << H(in.m_iShotsFired) << "\",\n";
            f << "  \"m_pCameraServices\": \"" << H(in.m_pCameraServices) << "\",\n";
            f << "  \"m_pBulletServices\": \"" << H(in.m_pBulletServices) << "\",\n";
            f << "  \"m_pClippingWeapon\": \"" << H(in.m_pClippingWeapon) << "\",\n";
            f << "  \"m_iIDEntIndex\": \"" << H(in.m_iIDEntIndex) << "\",\n";
            f << "  \"m_bIsScoped\": \"" << H(in.m_bIsScoped) << "\",\n";
            f << "  \"m_bIsDefusing\": \"" << H(in.m_bIsDefusing) << "\",\n";
            f << "  \"m_flFlashDuration\": \"" << H(in.m_flFlashDuration) << "\",\n";
            f << "  \"m_vecAbsOrigin\": \"" << H(in.m_vecAbsOrigin) << "\",\n";
            f << "  \"m_modelState\": \"" << H(in.m_modelState) << "\",\n";
            f << "  \"m_iFOVStart\": \"" << H(in.m_iFOVStart) << "\",\n";
            f << "  \"m_iszPlayerName\": \"" << H(in.m_iszPlayerName) << "\",\n";
            f << "  \"m_pMovementServices\": \"" << H(in.m_pMovementServices) << "\",\n";
            f << "  \"m_bInCrouch\": \"" << H(in.m_bInCrouch) << "\",\n";
            f << "  \"m_bDucked\": \"" << H(in.m_bDucked) << "\",\n";
            f << "  \"m_clrRender\": \"" << H(in.m_clrRender) << "\",\n";
            f << "  \"m_bGlowEnabled\": \"" << H(in.m_bGlowEnabled) << "\",\n";
            f << "  \"m_glowColorOverride\": \"" << H(in.m_glowColorOverride) << "\",\n";
            f << "  \"m_bBombTicking\": \"" << H(in.m_bBombTicking) << "\",\n";
            f << "  \"m_flC4Blow\": \"" << H(in.m_flC4Blow) << "\",\n";
            f << "  \"m_nBombSite\": \"" << H(in.m_nBombSite) << "\",\n";
            f << "  \"m_bBombPlanted\": \"" << H(in.m_bBombPlanted) << "\",\n";
            f << "  \"m_bBombDefused\": \"" << H(in.m_bBombDefused) << "\",\n";
            f << "  \"m_flTimerLength\": \"" << H(in.m_flTimerLength) << "\",\n";
            f << "  \"m_bBeingDefused\": \"" << H(in.m_bBeingDefused) << "\",\n";
            f << "  \"m_flDefuseCountDown\": \"" << H(in.m_flDefuseCountDown) << "\",\n";
            f << "  \"m_bBombDropped\": \"" << H(in.m_bBombDropped) << "\"\n";
            f << "}\n";
        } catch (...) {}
    }
};