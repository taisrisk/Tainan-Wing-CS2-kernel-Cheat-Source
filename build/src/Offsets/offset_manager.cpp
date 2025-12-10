#include "offset_manager.hpp"
#include <fstream>
#include <iostream>
#include "client.dll.hpp"

// Static member definitions
CS2Offsets OffsetsManager::offsets = {};
bool OffsetsManager::initialized = false;
const std::string OffsetsManager::cache_file = "cs2_offsets_cache.json";
// Force loading headers from the repository's local folder
const std::string OffsetsManager::client_dll_hpp_cache = "src/Offsets/client_dll.hpp";
const std::string OffsetsManager::offsets_hpp_cache = "src/Offsets/offsets.hpp";
const std::string OffsetsManager::buttons_hpp_cache = "src/Offsets/buttons.hpp";
