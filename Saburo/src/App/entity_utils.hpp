#pragma once

#include <cstdint>
#include "src/Core/driver.hpp"
#include "src/Offsets/offsets.hpp"

namespace EntityUtils {
    inline std::uintptr_t get_entity_from_handle(driver::DriverHandle& drv, std::uintptr_t client, uint32_t handle) {
        if (handle == 0) return 0;
        const uint32_t index = handle & 0x7FFF;
        std::uintptr_t entity_list = drv.read<std::uintptr_t>(client + cs2_dumper::offsets::client_dll::dwEntityList);
        if (entity_list == 0) return 0;
        const uint32_t chunk_index = index >> 9;
        std::uintptr_t chunk_pointer_address = entity_list + (0x8 * chunk_index) + 0x10;
        std::uintptr_t list_entry = drv.read<std::uintptr_t>(chunk_pointer_address);
        if (list_entry == 0) return 0;
        const uint32_t entity_index = index & 0x1FF;
        std::uintptr_t entity_address = list_entry + (0x70 * entity_index);
        std::uintptr_t entity = drv.read<std::uintptr_t>(entity_address);
        return entity;
    }
}
