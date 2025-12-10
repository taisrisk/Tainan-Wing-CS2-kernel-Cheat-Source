#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <iomanip>
#include "../Offsets/offset_manager.hpp"

// Legacy namespace for backward compatibility - redirects to OffsetsManager
// This allows existing code to keep using cs2_offsets:: syntax
namespace cs2_offsets {
    inline uint32_t m_hPlayerPawn() { return OffsetsManager::Get().m_hPlayerPawn; }
    inline uint32_t m_pGameSceneNode() { return OffsetsManager::Get().m_pGameSceneNode; }
    inline uint32_t m_iHealth() { return OffsetsManager::Get().m_iHealth; }
    inline uint32_t m_iMaxHealth() { return OffsetsManager::Get().m_iMaxHealth; }
    inline uint32_t m_lifeState() { return OffsetsManager::Get().m_lifeState; }
    inline uint32_t m_iTeamNum() { return OffsetsManager::Get().m_iTeamNum; }
    inline uint32_t m_vecAbsOrigin() { return OffsetsManager::Get().m_vecAbsOrigin; }
    inline uint32_t m_szLastPlaceName() { return 0x1B88; } // Static offset - kept as function for consistency
    inline uint32_t m_iszPlayerName() { return OffsetsManager::Get().m_iszPlayerName; }
    inline uint32_t m_iIDEntIndex() { return OffsetsManager::Get().m_iIDEntIndex; }
    inline uint32_t m_fFlags() { return OffsetsManager::Get().m_fFlags; }
}

// Basic 3D Vector
struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    float Distance(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// Entity data structure
struct Entity {
    std::uintptr_t address;      // Address of entity in game memory
    uint32_t handle;             // Entity handle
    Vector3 position;            // 3D position in world
    int32_t health;              // Current health
    int32_t max_health;          // Maximum health
    uint8_t team;                // Team (2=Terrorist, 3=CT, etc)
    uint8_t life_state;          // 0=alive, 1=dying, 2=dead
    std::string location;        // Location in map
    std::string name;            // Player name
    bool is_valid;               // Is this a valid/alive entity
    
    Entity() : address(0), handle(0), health(0), max_health(0), team(0), life_state(0), is_valid(false) {}
};

// Entity list manager
class EntityManager {
public:
    std::vector<Entity> all_entities;
    std::vector<Entity> enemy_entities;
    std::vector<Entity> teammate_entities;
    std::vector<Entity> alive_entities;
    std::vector<Entity> dead_entities;
    
    Entity local_player;
    uint8_t local_player_team;
    
    EntityManager() : local_player_team(0) {}
    
    void clear() {
        all_entities.clear();
        enemy_entities.clear();
        teammate_entities.clear();
        alive_entities.clear();
        dead_entities.clear();
    }
    
    void categorize_entities() {
        enemy_entities.clear();
        teammate_entities.clear();
        alive_entities.clear();
        dead_entities.clear();
        
        for (const auto& entity : all_entities) {
            if (!entity.is_valid) continue;
            
            if (entity.life_state == 0) {
                alive_entities.push_back(entity);
            } else {
                dead_entities.push_back(entity);
            }
            
            if (entity.team != local_player_team) {
                enemy_entities.push_back(entity);
            } else {
                teammate_entities.push_back(entity);
            }
        }
    }
    
    void print_summary() {
        std::cout << "\n===== ENTITY SUMMARY =====\n";
        std::cout << "Total Entities: " << all_entities.size() << "\n";
        std::cout << "Alive: " << alive_entities.size() << "\n";
        std::cout << "Dead: " << dead_entities.size() << "\n";
        std::cout << "Enemies: " << enemy_entities.size() << "\n";
        std::cout << "Teammates: " << teammate_entities.size() << "\n";
        std::cout << "Local Player Team: " << (int)local_player_team << "\n";
    }
    
    void print_entities() {
        std::cout << "\n===== ALL ENTITIES =====\n";
        for (size_t i = 0; i < all_entities.size(); ++i) {
            const auto& ent = all_entities[i];
            std::cout << "[" << i << "] ";
            std::cout << "Addr: 0x" << std::hex << ent.address << std::dec << " | ";
            std::cout << "Handle: 0x" << std::hex << ent.handle << std::dec << " | ";
            std::cout << "Pos: (" << std::fixed << std::setprecision(1) 
                      << ent.position.x << ", " << ent.position.y << ", " << ent.position.z << ") | ";
            std::cout << "HP: " << ent.health << "/" << ent.max_health << " | ";
            std::cout << "Team: " << (int)ent.team << " | ";
            std::cout << "State: " << (int)ent.life_state << " | ";
            std::cout << "Loc: " << ent.location << "\n";
        }
    }
    
    void print_alive_entities() {
        std::cout << "\n===== ALIVE ENTITIES =====\n";
        for (size_t i = 0; i < alive_entities.size(); ++i) {
            const auto& ent = alive_entities[i];
            std::cout << "[" << i << "] ";
            std::cout << "Addr: 0x" << std::hex << ent.address << std::dec << " | ";
            std::cout << "Pos: (" << std::fixed << std::setprecision(1) 
                      << ent.position.x << ", " << ent.position.y << ", " << ent.position.z << ") | ";
            std::cout << "HP: " << ent.health << "/" << ent.max_health << " | ";
            std::cout << "Team: " << (int)ent.team << " | ";
            std::cout << "Loc: " << ent.location << "\n";
        }
    }
};
