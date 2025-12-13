#pragma once

#include <string>
#include <cstdint>

struct CS2Offsets; // forward from offset_manager.hpp

namespace OffsetDecryptor {
    // Parses values directly from generated headers:
    // - src/Offsets/offsets.hpp (dw*)
    // - src/Offsets/buttons.hpp (attack/jump/left/right)
    // - src/Offsets/client_dll.hpp (m_* fields)
    // Returns true on success and fills out; optional log receives notes.
    bool DecryptHeadersTo(CS2Offsets& out, std::string* log = nullptr);
}
