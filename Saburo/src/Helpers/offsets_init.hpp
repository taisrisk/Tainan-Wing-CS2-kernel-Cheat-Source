#pragma once
#include <string>
namespace OffsetsInit {
    // Initialize offsets using header decryptor with version-aware cache.
    // If header parse fails, falls back to online JSON.
    bool Initialize(const std::string& appVersion);
}
