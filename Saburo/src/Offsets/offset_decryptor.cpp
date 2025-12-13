#include "offset_decryptor.hpp"
#include "offset_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <windows.h>

namespace {
    inline bool read_file(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    inline std::string get_exe_dir() {
        char buf[MAX_PATH]{};
        DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (n == 0) return std::string();
        std::string full(buf);
        size_t pos = full.find_last_of("\\/");
        if (pos == std::string::npos) return std::string();
        return full.substr(0, pos);
    }

    inline bool read_from_search_roots(const std::vector<std::string>& roots, const std::string& rel, std::string& out) {
        for (const auto& r : roots) {
            std::string path = r + "\\" + rel;
            if (read_file(path, out)) return true;
        }
        return false;
    }

    inline uint32_t parse_hex_after(const std::string& text, size_t pos) {
        // Expect pattern: 0x.... until ';'
        size_t hexPos = text.find("0x", pos);
        if (hexPos == std::string::npos) return 0;
        size_t end = text.find_first_of(";\n\r", hexPos);
        if (end == std::string::npos) end = text.size();
        std::string token = text.substr(hexPos, end - hexPos);
        try { return static_cast<uint32_t>(std::stoul(token, nullptr, 16)); } catch (...) { return 0; }
    }

    inline uint32_t extract_key(const std::string& text, const std::string& key) {
        // Look for "constexpr std::ptrdiff_t <key> = 0x...;"
        size_t pos = text.find(key);
        if (pos == std::string::npos) return 0;
        return parse_hex_after(text, pos);
    }

    inline void set(uint32_t& field, uint32_t val) { if (val) field = val; }
}

bool OffsetDecryptor::DecryptHeadersTo(CS2Offsets& out, std::string* log) {
    std::string off_hpp, btn_hpp, cdll_hpp;
    // Build search roots: exe dir and up to 4 parents
    const std::string base = get_exe_dir();
    std::vector<std::string> roots;
    roots.push_back(base);
    std::string cur = base;
    for (int i = 0; i < 4; ++i) {
        size_t pos = cur.find_last_of("\\/");
        if (pos == std::string::npos) break;
        cur = cur.substr(0, pos);
        roots.push_back(cur);
    }

    bool ok1 = read_from_search_roots(roots, "src\\Offsets\\offsets.hpp", off_hpp) ||
               read_from_search_roots(roots, "Offsets\\offsets.hpp", off_hpp);
    bool ok2 = read_from_search_roots(roots, "src\\Offsets\\buttons.hpp", btn_hpp) ||
               read_from_search_roots(roots, "Offsets\\buttons.hpp", btn_hpp);
    bool ok3 = read_from_search_roots(roots, "src\\Offsets\\client_dll.hpp", cdll_hpp) ||
               read_from_search_roots(roots, "Offsets\\client_dll.hpp", cdll_hpp);
    if (log) {
        *log += std::string("[decryptor] load offsets.hpp: ") + (ok1?"ok":"fail") + "\n";
        *log += std::string("[decryptor] load buttons.hpp: ") + (ok2?"ok":"fail") + "\n";
        *log += std::string("[decryptor] load client_dll.hpp: ") + (ok3?"ok":"fail") + "\n";
        *log += std::string("[decryptor] search roots include: ") + base + " (parents up to 4)\n";
    }
    if (!(ok1 && ok2 && ok3)) return false;

    // Global dw* from offsets.hpp
    set(out.dwEntityList,           extract_key(off_hpp, "dwEntityList"));
    set(out.dwViewMatrix,           extract_key(off_hpp, "dwViewMatrix"));
    set(out.dwViewAngles,           extract_key(off_hpp, "dwViewAngles"));
    set(out.dwLocalPlayerPawn,      extract_key(off_hpp, "dwLocalPlayerPawn"));
    set(out.dwLocalPlayerController,extract_key(off_hpp, "dwLocalPlayerController"));
    set(out.dwGlobalVars,           extract_key(off_hpp, "dwGlobalVars"));
    set(out.dwPlantedC4,            extract_key(off_hpp, "dwPlantedC4"));
    set(out.dwCSGOInput,            extract_key(off_hpp, "dwCSGOInput"));
    set(out.dwInputSystem,          extract_key(off_hpp, "dwInputSystem"));
    set(out.dwSensitivity,          extract_key(off_hpp, "dwSensitivity"));
    set(out.dwSensitivity_sensitivity, extract_key(off_hpp, "dwSensitivity_sensitivity"));

    // Buttons
    set(out.buttons.attack,         extract_key(btn_hpp, "attack"));
    set(out.buttons.jump,           extract_key(btn_hpp, "jump"));
    set(out.buttons.right,          extract_key(btn_hpp, "right"));
    set(out.buttons.left,           extract_key(btn_hpp, "left"));

    // Entity / Pawn fields from client_dll.hpp (search everywhere by key)
    set(out.m_iHealth,              extract_key(cdll_hpp, "m_iHealth"));
    set(out.m_iMaxHealth,           extract_key(cdll_hpp, "m_iMaxHealth"));
    set(out.m_iTeamNum,             extract_key(cdll_hpp, "m_iTeamNum"));
    set(out.m_fFlags,               extract_key(cdll_hpp, "m_fFlags"));
    set(out.m_vecAbsVelocity,       extract_key(cdll_hpp, "m_vecAbsVelocity"));
    set(out.m_pGameSceneNode,       extract_key(cdll_hpp, "m_pGameSceneNode"));
    set(out.m_lifeState,            extract_key(cdll_hpp, "m_lifeState"));
    set(out.m_hPlayerPawn,          extract_key(cdll_hpp, "m_hPlayerPawn"));
    set(out.m_angEyeAngles,         extract_key(cdll_hpp, "m_angEyeAngles"));
    set(out.m_vecLastClipCameraPos, extract_key(cdll_hpp, "m_vecLastClipCameraPos"));
    set(out.m_aimPunchAngle,        extract_key(cdll_hpp, "m_aimPunchAngle"));
    set(out.m_iShotsFired,          extract_key(cdll_hpp, "m_iShotsFired"));
    set(out.m_pCameraServices,      extract_key(cdll_hpp, "m_pCameraServices"));
    set(out.m_pBulletServices,      extract_key(cdll_hpp, "m_pBulletServices"));
    set(out.m_pClippingWeapon,      extract_key(cdll_hpp, "m_pClippingWeapon"));
    set(out.m_iIDEntIndex,          extract_key(cdll_hpp, "m_iIDEntIndex"));
    set(out.m_bIsScoped,            extract_key(cdll_hpp, "m_bIsScoped"));
    set(out.m_bIsDefusing,          extract_key(cdll_hpp, "m_bIsDefusing"));
    set(out.m_flFlashDuration,      extract_key(cdll_hpp, "m_flFlashDuration"));
    set(out.m_vecAbsOrigin,         extract_key(cdll_hpp, "m_vecAbsOrigin"));
    set(out.m_modelState,           extract_key(cdll_hpp, "m_modelState"));
    set(out.m_iFOVStart,            extract_key(cdll_hpp, "m_iFOVStart"));
    set(out.m_iszPlayerName,        extract_key(cdll_hpp, "m_iszPlayerName"));
    set(out.m_pMovementServices,    extract_key(cdll_hpp, "m_pMovementServices"));
    set(out.m_bInCrouch,            extract_key(cdll_hpp, "m_bInCrouch"));
    set(out.m_bDucked,              extract_key(cdll_hpp, "m_bDucked"));
    set(out.m_clrRender,            extract_key(cdll_hpp, "m_clrRender"));
    set(out.m_bGlowEnabled,         extract_key(cdll_hpp, "m_bGlowEnabled"));
    set(out.m_glowColorOverride,    extract_key(cdll_hpp, "m_glowColorOverride"));
    set(out.m_bBombTicking,         extract_key(cdll_hpp, "m_bBombTicking"));
    set(out.m_flC4Blow,             extract_key(cdll_hpp, "m_flC4Blow"));
    set(out.m_nBombSite,            extract_key(cdll_hpp, "m_nBombSite"));
    set(out.m_flTimerLength,        extract_key(cdll_hpp, "m_flTimerLength"));
    set(out.m_bBeingDefused,        extract_key(cdll_hpp, "m_bBeingDefused"));
    set(out.m_flDefuseCountDown,    extract_key(cdll_hpp, "m_flDefuseCountDown"));
    set(out.m_bBombDefused,         extract_key(cdll_hpp, "m_bBombDefused"));
    set(out.m_bBombPlanted,         extract_key(cdll_hpp, "m_bBombPlanted"));
    set(out.m_bBombDropped,         extract_key(cdll_hpp, "m_bBombDropped"));

    // Sanity fallbacks (preserve previous OffsetsManager defaults if still zero)
    if (!out.m_lifeState) out.m_lifeState = 0x354;
    if (!out.m_pMovementServices) out.m_pMovementServices = 0x1138;
    if (!out.m_bInCrouch) out.m_bInCrouch = 0x240;
    if (!out.m_bDucked) out.m_bDucked = 0x24C;
    if (!out.m_clrRender) out.m_clrRender = 0x7C0;
    if (!out.m_bGlowEnabled) out.m_bGlowEnabled = 0x444;
    if (!out.m_glowColorOverride) out.m_glowColorOverride = 0x468;
    if (!out.m_bBombTicking) out.m_bBombTicking = 0xED0;
    if (!out.m_flC4Blow) out.m_flC4Blow = 0xF00;
    if (!out.m_nBombSite) out.m_nBombSite = 0xED4;
    if (!out.m_flTimerLength) out.m_flTimerLength = 0xF08;
    if (!out.m_bBeingDefused) out.m_bBeingDefused = 0xF0C;
    if (!out.m_flDefuseCountDown) out.m_flDefuseCountDown = 0xF20;
    if (!out.m_bBombDefused) out.m_bBombDefused = 0xF24;
    if (!out.m_bBombPlanted) out.m_bBombPlanted = 0x9A5;
    if (!out.m_bBombDropped) out.m_bBombDropped = 0x9A4;

    if (log) *log += "[decryptor] header parse complete\n";
    return true;
}
