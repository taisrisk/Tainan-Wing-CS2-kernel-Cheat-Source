#include "offsets_init.hpp"
#include "../Offsets/offset_manager.hpp"
#include "console_colors.hpp"

namespace OffsetsInit {

bool Initialize(const std::string& appVersion) {
    ConsoleColor::PrintInfo("Initializing offsets...");
    // 1) Prefer local JSON (src/Offsets/*.json) - Nova-style
    bool ok = OffsetsManager::InitializeFromLocalJson();
    // 2) Fallback to header decryptor + cache
    if (!ok) ok = OffsetsManager::InitializeFromHeadersCached(appVersion);
    // 3) Fallback to remote JSON download/cache
    if (!ok) ok = OffsetsManager::Initialize();
    if (!ok) {
        ConsoleColor::PrintError("Failed to initialize offsets");
        return false;
    }
    ConsoleColor::PrintSuccess("Offsets initialized");
    return true;
}

}
