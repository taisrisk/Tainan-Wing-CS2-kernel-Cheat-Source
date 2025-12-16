#include "offset_manager.hpp"
#include <fstream>
#include <iostream>

// Static member definitions
CS2Offsets OffsetsManager::offsets = {};
bool OffsetsManager::initialized = false;
const std::string OffsetsManager::cache_file = "cs2_offsets_cache.json";
const std::string OffsetsManager::client_dll_hpp_cache = "client_dll.hpp";
const std::string OffsetsManager::header_cache_file = "cs2_offsets_header_cache.json";
