#pragma once

#include "entity.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

// ===== CRITICAL FIX =====
// The position offset was WRONG. CS2 uses m_vOrigin NOT m_vecAbsOrigin
// m_vOrigin is at offset 0x130 within the pawn entity

namespace cs2_pos_offsets {
    // These are the CORRECT offsets for reading position in CS2
    // Position is stored at entity + 0x130 as a Vector3
    constexpr std::ptrdiff_t m_vOrigin = 0x130;
}

class EntityEnumerator {
private:
    driver::DriverHandle& drv;
    std::uintptr_t client_base;

    static std::uintptr_t get_entity_from_handle(driver::DriverHandle& drv, std::uintptr_t client, uint32_t handle) {
        if (handle == 0) return 0;
        const uint32_t index = handle & 0x7FFF;
        std::uintptr_t entity_list = drv.read<std::uintptr_t>(client + OffsetsManager::Get().dwEntityList);
        if (entity_list == 0) return 0;
        const uint32_t chunk_index = index >> 9;
        std::uintptr_t chunk_pointer_address = entity_list + (0x8 * chunk_index) + 0x10;
        std::uintptr_t list_entry = drv.read<std::uintptr_t>(chunk_pointer_address);
        if (list_entry == 0) return 0;
        const uint32_t entity_index = index & 0x1FF;
        std::uintptr_t entity_address = list_entry + (0x70 * entity_index);
        return drv.read<std::uintptr_t>(entity_address);
    }

    // Cache valid entity indices across frames for performance
    std::vector<uint32_t> knownEntityIndices;
    bool hasInitialScan = false;

    // Validate if health value is realistic
    bool is_health_valid(int health) const {
        // STRICT: Only accept valid in-game health values
        return health > 0 && health <= 150;  // 150 for armor/heavy
    }

    // Validate if team is valid (CT=2, T=3)
    bool is_team_valid(uint8_t team) const {
        return team == 2 || team == 3;
    }

    // Check if position has valid values (no NaN, no extreme values)
    bool is_position_valid(const Vector3& pos) const {
        if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z)) return false;
        if (std::isinf(pos.x) || std::isinf(pos.y) || std::isinf(pos.z)) return false;

        // Check if coordinates are reasonable (map is roughly -5000 to 5000 units)
        if (std::abs(pos.x) > 20000 || std::abs(pos.y) > 20000 || std::abs(pos.z) > 5000) return false;

        // Check if position is not at origin (0,0,0) - usually indicates invalid entity
        if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) return false;

        return true;
    }

    // Quick check if an entity is likely a player (without full validation)
    bool is_likely_player(std::uintptr_t entity_address) {
        if (entity_address == 0) return false;

        // Quick health check (most non-player entities have 0 or invalid health)
        int health = drv.read<int32_t>(entity_address + cs2_offsets::m_iHealth());
        if (health <= 0 || health > 150) return false;  // Changed from 150 to 100

        // Quick team check
        uint8_t team = drv.read<uint8_t>(entity_address + cs2_offsets::m_iTeamNum());
        if (team != 2 && team != 3) return false;

        return true;
    }

public:
    EntityEnumerator(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), client_base(client) {
    }

    // Force a complete rescan of entities (used when entity list drops to 0)
    void force_rescan(EntityManager& manager, std::uintptr_t local_player_address) {
        // Clear all caches
        hasInitialScan = false;
        knownEntityIndices.clear();
        manager.clear();

        // Get updated local player info
        Entity local_player = read_entity_data(local_player_address);
        manager.local_player = local_player;
        manager.local_player_team = local_player.team;

        // Perform fresh enumeration
        std::uintptr_t entity_list = drv.read<std::uintptr_t>(client_base + OffsetsManager::Get().dwEntityList);

        if (entity_list == 0) {
            return;
        }

        // Full scan with fresh cache
        for (uint32_t i = 0; i < 8192; ++i) {
            uint32_t chunk_index = i >> 9;
            uint32_t entity_index = i & 0x1FF;

            std::uintptr_t chunk_ptr_address = entity_list + (0x8 * chunk_index) + 0x10;
            std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);

            if (chunk_ptr == 0) {
                i = ((chunk_index + 1) << 9) - 1;
                continue;
            }

            std::uintptr_t entity_address = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entity_index));
            if (entity_address == 0) continue;

            if (!is_likely_player(entity_address)) continue;

            Entity entity = read_entity_data(entity_address);
            if (entity.is_valid) {
                manager.all_entities.push_back(entity);
                knownEntityIndices.push_back(i);
            }
        }

        hasInitialScan = true;
        manager.categorize_entities();
    }

    Entity read_entity_data(std::uintptr_t entity_address, bool lightweight = false) {
        Entity entity;
        entity.address = entity_address;

        if (entity_address == 0) {
            return entity;
        }

        try {
            // Read health and validate first
            entity.health = drv.read<int32_t>(entity_address + cs2_offsets::m_iHealth());
            entity.life_state = drv.read<uint8_t>(entity_address + cs2_offsets::m_lifeState());
            entity.team = drv.read<uint8_t>(entity_address + cs2_offsets::m_iTeamNum());
            if (!lightweight) {
                entity.max_health = drv.read<int32_t>(entity_address + cs2_offsets::m_iMaxHealth());
            }

            // ===== SCENE NODE METHOD =====
            // Read position through the scene node (more reliable)
            // 1. Get scene node pointer from pawn
            std::uintptr_t scene_node = drv.read<std::uintptr_t>(entity_address + cs2_offsets::m_pGameSceneNode());

            if (scene_node != 0) {
                // 2. Read position from scene node at correct offset (0xD0)
                float pos_array[3] = { 0, 0, 0 };
                drv.read_memory(
                    reinterpret_cast<PVOID>(scene_node + cs2_offsets::m_vecAbsOrigin()),
                    pos_array,
                    sizeof(pos_array)
                );
                entity.position = Vector3(pos_array[0], pos_array[1], pos_array[2]);
            }

            if (!lightweight) {
                char location_buffer[256] = { 0 };
                drv.read_memory(reinterpret_cast<PVOID>(entity_address + cs2_offsets::m_szLastPlaceName()), location_buffer, sizeof(location_buffer));
                entity.location = std::string(location_buffer);
            }

            const auto& offsets = OffsetsManager::Get();

            {
                auto read_ascii_string = [&](std::uintptr_t base, size_t maxLen) -> std::string {
                    if (base == 0) return {};
                    std::string out;
                    out.reserve(maxLen);
                    for (size_t i = 0; i < maxLen; i++) {
                        char c = drv.read<char>(base + i);
                        if (c == '\0') break;
                        if (c < 32 || c > 126) return {};
                        out.push_back(c);
                    }
                    if (out.size() < 2 || out.size() > 64) return {};
                    if (out.find_first_not_of(" ") == std::string::npos) return {};
                    return out;
                };

                entity.name.clear();

                std::uintptr_t controller = 0;
                try {
                    uint32_t pawnHandle = drv.read<uint32_t>(entity_address + offsets.m_hPlayerPawn);
                    if (pawnHandle != 0) {
                        controller = entity_address;
                    }
                }
                catch (...) {
                    controller = 0;
                }

                if (controller == 0) {
                    try {
                        std::uintptr_t localController = drv.read<std::uintptr_t>(client_base + offsets.dwLocalPlayerController);
                        if (localController != 0) {
                            uint32_t localPawnHandle = drv.read<uint32_t>(localController + offsets.m_hPlayerPawn);
                            if (localPawnHandle != 0) {
                                std::uintptr_t localPawn = get_entity_from_handle(drv, client_base, localPawnHandle);
                                if (localPawn == entity_address) {
                                    controller = localController;
                                }
                            }
                        }
                    }
                    catch (...) {
                        controller = 0;
                    }
                }

                if (controller != 0) {
                    std::uintptr_t nameBase = controller + offsets.m_iszPlayerName;
                    if (offsets.m_iszPlayerName == 0) {
                        nameBase = controller + 0x6E8;
                    }
                    entity.name = read_ascii_string(nameBase, 127);

                    if (entity.name.empty()) {
                        std::uintptr_t sanitizedBase = controller + 0x850;
                        entity.name = read_ascii_string(sanitizedBase, 127);
                    }
                }
            }

            if (lightweight) {
                entity.is_valid = (
                    entity_address != 0 &&
                    is_health_valid(entity.health) &&
                    is_team_valid(entity.team) &&
                    is_position_valid(entity.position)
                    );
            } else {
                entity.is_valid = (
                    entity_address != 0 &&
                    is_health_valid(entity.health) &&
                    is_health_valid(entity.max_health) &&
                    is_team_valid(entity.team) &&
                    is_position_valid(entity.position)
                    );
            }

        }
        catch (...) {
            entity.is_valid = false;
        }

        return entity;
    }

    // Enumerate all entities in the world (initial scan)
    EntityManager enumerate_all_entities(std::uintptr_t local_player_address) {
        EntityManager manager;

        // Get local player info
        Entity local_player = read_entity_data(local_player_address);
        manager.local_player = local_player;
        manager.local_player_team = local_player.team;

        // Read entity list
        std::uintptr_t entity_list = drv.read<std::uintptr_t>(client_base + OffsetsManager::Get().dwEntityList);

        if (entity_list == 0) {
            return manager;
        }

        // Clear cache for fresh scan
        knownEntityIndices.clear();

        // Scan entities
        for (uint32_t i = 0; i < 8192; ++i) {
            uint32_t chunk_index = i >> 9;
            uint32_t entity_index = i & 0x1FF;

            std::uintptr_t chunk_ptr_address = entity_list + (0x8 * chunk_index) + 0x10;
            std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);

            if (chunk_ptr == 0) {
                i = ((chunk_index + 1) << 9) - 1;
                continue;
            }

            std::uintptr_t entity_address = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entity_index));

            if (entity_address == 0) {
                continue;
            }

            if (!is_likely_player(entity_address)) {
                continue;
            }

            Entity entity = read_entity_data(entity_address);

            if (entity.is_valid) {
                manager.all_entities.push_back(entity);
                knownEntityIndices.push_back(i);
            }
        }

        hasInitialScan = true;
        manager.categorize_entities();

        return manager;
    }

    // Update entities every frame (for live updates) - OPTIMIZED WITH CACHING
    void update_entities(EntityManager& manager) {
        // Clear only the all_entities vector, keep local player info
        manager.all_entities.clear();

        std::uintptr_t entity_list = drv.read<std::uintptr_t>(client_base + OffsetsManager::Get().dwEntityList);
        if (entity_list == 0) return;

        // If we have cached indices, use them for ultra-fast updates
        if (hasInitialScan && !knownEntityIndices.empty()) {
            // Fast path: Only check known player slots
            for (uint32_t i : knownEntityIndices) {
                uint32_t chunk_index = i >> 9;
                uint32_t entity_index = i & 0x1FF;

                std::uintptr_t chunk_ptr_address = entity_list + (0x8 * chunk_index) + 0x10;
                std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);

                if (chunk_ptr == 0) continue;

                std::uintptr_t entity_address = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entity_index));
                if (entity_address == 0) continue;

                Entity entity = read_entity_data(entity_address, true);

                if (entity.is_valid) {
                    manager.all_entities.push_back(entity);
                }
            }

            // Periodically rescan to detect new players
            static int frame_counter = 0;
            frame_counter++;
            if (frame_counter >= 60) {
                frame_counter = 0;
                rescan_for_new_players(manager, entity_list);
            }
        }
        else {
            // Fallback: Full scan if cache not initialized
            scan_all_entities(manager, entity_list);
        }

        manager.categorize_entities();
    }

private:
    // Rescan for new players without full entity list traversal
    void rescan_for_new_players(EntityManager& manager, std::uintptr_t entity_list) {
        std::vector<uint32_t> newIndices;

        // Quick scan of first 2048 slots looking for new players
        for (uint32_t i = 0; i < 8192; ++i) {
            // Skip if already cached
            if (std::find(knownEntityIndices.begin(), knownEntityIndices.end(), i) != knownEntityIndices.end()) {
                continue;
            }

            uint32_t chunk_index = i >> 9;
            uint32_t entity_index = i & 0x1FF;

            std::uintptr_t chunk_ptr_address = entity_list + (0x8 * chunk_index) + 0x10;
            std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);

            if (chunk_ptr == 0) {
                // Skip rest of chunk
                i = ((chunk_index + 1) << 9) - 1;
                continue;
            }

            std::uintptr_t entity_address = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entity_index));
            if (entity_address == 0) continue;

            if (!is_likely_player(entity_address)) continue;

            Entity entity = read_entity_data(entity_address);
            if (entity.is_valid) {
                manager.all_entities.push_back(entity);
                newIndices.push_back(i);
            }
        }

        // Add new indices to cache
        if (!newIndices.empty()) {
            knownEntityIndices.insert(knownEntityIndices.end(), newIndices.begin(), newIndices.end());
        }
    }

    // Full entity scan (used as fallback)
    void scan_all_entities(EntityManager& manager, std::uintptr_t entity_list) {
        knownEntityIndices.clear();

        for (uint32_t i = 0; i < 8192; ++i) {
            uint32_t chunk_index = i >> 9;
            uint32_t entity_index = i & 0x1FF;

            std::uintptr_t chunk_ptr_address = entity_list + (0x8 * chunk_index) + 0x10;
            std::uintptr_t chunk_ptr = drv.read<std::uintptr_t>(chunk_ptr_address);

            if (chunk_ptr == 0) {
                i = ((chunk_index + 1) << 9) - 1;
                continue;
            }

            std::uintptr_t entity_address = drv.read<std::uintptr_t>(chunk_ptr + (0x70 * entity_index));
            if (entity_address == 0) continue;

            if (!is_likely_player(entity_address)) continue;

            Entity entity = read_entity_data(entity_address);
            if (entity.is_valid) {
                manager.all_entities.push_back(entity);
                knownEntityIndices.push_back(i);
            }
        }

        hasInitialScan = true;
    }
};
