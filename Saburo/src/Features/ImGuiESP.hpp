#pragma once
#include "../Helpers/OS-ImGui/OS-ImGui.h"
#include "entity.hpp"
#include "HeadAngleLine.hpp"
#include "VisibilityChecker.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include <memory>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <vector>

class ImGuiESP {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    
    EntityManager entityBuffer[2];
    std::atomic<int> readIndex{0};
    std::atomic<int> writeIndex{1};
    std::uintptr_t localPlayerAddress = 0;
    
    float viewMatrix[16] = {0};
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;
    
    std::chrono::steady_clock::time_point lastFrameTime;
    float currentFPS = 0.0f;
    int frameCount = 0;
    
    HeadAngleLine headAngleLine;
    VisibilityChecker visibilityChecker;
    bool snaplinesEnabled = false;
    bool snaplinesWallCheckEnabled = false;
    bool distanceESPEnabled = false;
    bool chamsEnabled = false;
    bool lobbyMode = false;
    
    // Visibility cache for snaplines
    std::vector<bool> enemyVisibility;
    std::vector<bool> teammateVisibility;
    
public:
    ImGuiESP(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), headAngleLine(driver, client), visibilityChecker(driver, client) {
        lastFrameTime = std::chrono::steady_clock::now();
        headAngleLine.setScreenDimensions(screenWidth, screenHeight);
    }
    
    void updateEntities(const EntityManager& entities, std::uintptr_t localAddr) {
        int writeIdx = writeIndex.load(std::memory_order_relaxed);
        entityBuffer[writeIdx] = entities;
        localPlayerAddress = localAddr;
        
        // Update head angle line local player
        headAngleLine.setLocalPlayerPawn(localAddr);
        
        // Update visibility cache if wall-check is enabled
        if (snaplinesWallCheckEnabled && localAddr != 0) {
            visibilityChecker.updateVisibility(entities, localAddr, enemyVisibility, teammateVisibility);
        }
        
        int oldRead = readIndex.load(std::memory_order_relaxed);
        readIndex.store(writeIdx, std::memory_order_release);
        writeIndex.store(oldRead, std::memory_order_relaxed);
    }
    
    void setHeadAngleLineEnabled(bool enable) { headAngleLine.setEnabled(enable); }
    bool isHeadAngleLineEnabled() const { return headAngleLine.isEnabled(); }
    void setHeadAngleLineTeamCheckEnabled(bool enable) { headAngleLine.setTeamCheckEnabled(enable); }
    void setSnaplinesEnabled(bool enable) { snaplinesEnabled = enable; }
    bool isSnaplinesEnabled() const { return snaplinesEnabled; }
    void setSnaplinesWallCheckEnabled(bool enable) { snaplinesWallCheckEnabled = enable; }
    bool isSnaplinesWallCheckEnabled() const { return snaplinesWallCheckEnabled; }
    void setDistanceESPEnabled(bool enable) { distanceESPEnabled = enable; }
    bool isDistanceESPEnabled() const { return distanceESPEnabled; }
    void setChamsEnabled(bool enable) { chamsEnabled = enable; }
    bool isChamsEnabled() const { return chamsEnabled; }
    void setLobbyMode(bool enable) { lobbyMode = enable; }
    bool isLobbyMode() const { return lobbyMode; }
    
    void updateViewMatrix() {
        std::uintptr_t matrixAddress = clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix;
        drv.read_memory(reinterpret_cast<PVOID>(matrixAddress), viewMatrix, sizeof(viewMatrix));
    }
    
    void render() {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        frameCount++;
        if (frameCount >= 60) {
            currentFPS = 1.0f / deltaTime;
            frameCount = 0;
        }
        
        auto* drawList = ImGui::GetBackgroundDrawList();
        
        // DYNAMIC SCREEN BORDER: Blue in lobby, Red in-game
        ImColor borderColor = lobbyMode ? ImColor(0, 120, 255, 255) : ImColor(255, 0, 0, 255);
        float borderThickness = 3.0f;
        
        // Draw complete border around entire screen
        ImVec2 screenMin(0, 0);
        ImVec2 screenMax(screenWidth, screenHeight);
        drawList->AddRect(screenMin, screenMax, borderColor, 0.0f, 0, borderThickness);
        
        // DRAW HEAD ANGLE LINE (perfect headshot aim line)
        if (headAngleLine.isEnabled() && !lobbyMode) {
            int readIdx = readIndex.load(std::memory_order_acquire);
            const EntityManager& currentEntities = entityBuffer[readIdx];
            
            // Update view matrix for head angle line
            headAngleLine.updateViewMatrix(viewMatrix);
            
            auto linePos = headAngleLine.calculateLinePosition(currentEntities);
            if (linePos.valid) {
                ImColor lineColor(0, 255, 0, 255); // Green line
                ImColor shadowColor(0, 0, 0, 128); // Black shadow
                
                // Left rectangles (shadow + main)
                drawList->AddRectFilled(ImVec2(linePos.x - 21, linePos.y - 1), ImVec2(linePos.x - 4, linePos.y + 2), shadowColor);
                drawList->AddRectFilled(ImVec2(linePos.x - 20, linePos.y), ImVec2(linePos.x - 3, linePos.y + 3), lineColor);
                
                // Right rectangles (shadow + main)
                drawList->AddRectFilled(ImVec2(linePos.x + 5, linePos.y - 1), ImVec2(linePos.x + 22, linePos.y + 2), shadowColor);
                drawList->AddRectFilled(ImVec2(linePos.x + 6, linePos.y), ImVec2(linePos.x + 23, linePos.y + 3), lineColor);
            }
        }
        
        // LOBBY MODE: Draw ESP on local player only
        if (lobbyMode && localPlayerAddress != 0) {
            const auto& offsets = OffsetsManager::Get();
            
            // Get local player position
            std::uintptr_t localSceneNode = drv.read<std::uintptr_t>(localPlayerAddress + offsets.m_pGameSceneNode);
            if (localSceneNode != 0) {
                Vector3 localPos;
                drv.read_memory(reinterpret_cast<void*>(localSceneNode + offsets.m_vecAbsOrigin), &localPos, sizeof(Vector3));
                
                int32_t health = drv.read<int32_t>(localPlayerAddress + offsets.m_iHealth);
                
                // Create a temporary entity for the local player
                Entity localEntity;
                localEntity.address = localPlayerAddress;
                localEntity.position = localPos;
                localEntity.health = health;
                localEntity.max_health = 100;
                localEntity.is_valid = true;
                localEntity.name = "YOU (Lobby)";
                
                // Draw lobby ESP with cyan color
                drawEntity(localEntity, ImColor(0, 255, 255, 255), false, true);
            }
            
            // Show lobby message
            drawList->AddText(ImVec2(10, 10), ImColor(0, 255, 255, 255), "LOBBY MODE - Waiting for game...");
            return;  // Don't render anything else in lobby mode
        }
        
        // NORMAL MODE: Render all entities
        char fpsText[64];
        snprintf(fpsText, sizeof(fpsText), "ESP FPS: %.0f", currentFPS);
        drawList->AddText(ImVec2(10, 10), ImColor(0, 255, 0, 255), fpsText);
        
        int readIdx = readIndex.load(std::memory_order_acquire);
        const EntityManager& currentEntities = entityBuffer[readIdx];
        
        char countText[64];
        snprintf(countText, sizeof(countText), "Entities: %zu", currentEntities.enemy_entities.size() + currentEntities.teammate_entities.size());
        drawList->AddText(ImVec2(10, 30), ImColor(255, 255, 255, 255), countText);
        
        // Render enemies with visibility index
        for (size_t i = 0; i < currentEntities.enemy_entities.size(); i++) {
            const auto& ent = currentEntities.enemy_entities[i];
            if (!ent.is_valid || ent.address == localPlayerAddress) continue;
            
            bool visible = (snaplinesWallCheckEnabled && i < enemyVisibility.size()) ? enemyVisibility[i] : true;
            drawEntity(ent, ImColor(255, 0, 0, 255), true, visible);
        }
        
        // Render teammates with visibility index
        for (size_t i = 0; i < currentEntities.teammate_entities.size(); i++) {
            const auto& ent = currentEntities.teammate_entities[i];
            if (!ent.is_valid || ent.address == localPlayerAddress) continue;
            
            bool visible = (snaplinesWallCheckEnabled && i < teammateVisibility.size()) ? teammateVisibility[i] : true;
            drawEntity(ent, ImColor(0, 0, 255, 255), false, visible);
        }
    }
    
private:
    bool worldToScreen(const Vector3& world, Vec2& screen) {
        float w = viewMatrix[12] * world.x + viewMatrix[13] * world.y + viewMatrix[14] * world.z + viewMatrix[15];
        if (w < 0.01f) return false;
        
        float x = viewMatrix[0] * world.x + viewMatrix[1] * world.y + viewMatrix[2] * world.z + viewMatrix[3];
        float y = viewMatrix[4] * world.x + viewMatrix[5] * world.y + viewMatrix[6] * world.z + viewMatrix[7];
        
        x /= w;
        y /= w;
        
        screen.x = (screenWidth / 2.0f) * (1.0f + x);
        screen.y = (screenHeight / 2.0f) * (1.0f - y);
        
        return (screen.x >= 0 && screen.x <= screenWidth && screen.y >= 0 && screen.y <= screenHeight);
    }
    
    void drawEntity(const Entity& ent, ImColor color, bool isEnemy, bool isVisible) {
        Vector3 headPos = ent.position;
        headPos.z += 70.0f;  // Reduced from 75.0f to better fit player model
        
        Vector3 feetPos = ent.position;
        feetPos.z -= 5.0f;
        
        Vec2 headScreen, feetScreen;
        bool onScreen = worldToScreen(headPos, headScreen) && worldToScreen(feetPos, feetScreen);
        
        auto* drawList = ImGui::GetBackgroundDrawList();
        
        // Draw snaplines FIRST - even if entity is off-screen
        if (snaplinesEnabled) {
            // If wall-check is enabled, only draw if visible
            if (!snaplinesWallCheckEnabled || isVisible) {
                // Calculate screen position for snapline endpoint
                Vec2 snapEndpoint;
                
                // If entity is on screen, use HEAD position (top of box)
                if (onScreen) {
                    snapEndpoint = headScreen;  // CHANGED FROM feetScreen TO headScreen
                } else {
                    // If off-screen, try to get ANY position on screen
                    // Use the raw position even if worldToScreen says it's invalid
                    Vector3 centerPos = ent.position;
                    Vec2 centerScreen;
                    worldToScreen(centerPos, centerScreen);
                    
                    // Clamp to screen edges  
                    float clampedX = centerScreen.x;
                    if (clampedX < 0.0f) clampedX = 0.0f;
                    if (clampedX > screenWidth) clampedX = screenWidth;
                    
                    float clampedY = centerScreen.y;
                    if (clampedY < 0.0f) clampedY = 0.0f;
                    if (clampedY > screenHeight) clampedY = screenHeight;
                    
                    snapEndpoint.x = clampedX;
                    snapEndpoint.y = clampedY;
                }
                
                // Draw from TOP of screen to TOP of entity box
                ImVec2 startPos(screenWidth / 2.0f, 0.0f);  // TOP CENTER
                ImVec2 endPos(snapEndpoint.x, snapEndpoint.y);
                ImColor lineColor(color.Value.x, color.Value.y, color.Value.z, 0.6f);
                drawList->AddLine(startPos, endPos, lineColor, 1.5f);
            }
        }
        
        // Only draw box and other elements if on screen
        if (!onScreen) return;
        
        float height = feetScreen.y - headScreen.y;
        float width = height * 0.5f;
        
        Vec2 boxPos(headScreen.x - width / 2, headScreen.y);
        Vec2 boxSize(width, height);
        
        // CHAMS: Draw glowing outline around entity (not filled box)
        if (chamsEnabled) {
            ImColor chamsColor = isEnemy ? ImColor(255, 0, 0, 200) : ImColor(0, 0, 255, 200);
            
            // Draw thick glowing border around entity
            drawList->AddRect(
                ImVec2(boxPos.x - 3, boxPos.y - 3), 
                ImVec2(boxPos.x + boxSize.x + 3, boxPos.y + boxSize.y + 3), 
                chamsColor, 
                4.0f, 0, 5.0f  // Thick glow effect
            );
            
            // Second layer for stronger glow
            ImColor glowOuter = isEnemy ? ImColor(255, 0, 0, 100) : ImColor(0, 0, 255, 100);
            drawList->AddRect(
                ImVec2(boxPos.x - 6, boxPos.y - 6), 
                ImVec2(boxPos.x + boxSize.x + 6, boxPos.y + boxSize.y + 6), 
                glowOuter, 
                4.0f, 0, 3.0f
            );
        }
        
        // Draw entity box AFTER chams so box is visible
        drawList->AddRect(boxPos.ToImVec2(), ImVec2(boxPos.x + boxSize.x, boxPos.y + boxSize.y), color, 4.0f, 0, 2.0f);
        
        // Draw distance ESP if enabled
        if (distanceESPEnabled && localPlayerAddress != 0) {
            const auto& offsets = OffsetsManager::Get();
            std::uintptr_t localSceneNode = drv.read<std::uintptr_t>(localPlayerAddress + offsets.m_pGameSceneNode);
            if (localSceneNode != 0) {
                Vector3 localPos;
                drv.read_memory(reinterpret_cast<void*>(localSceneNode + offsets.m_vecAbsOrigin), &localPos, sizeof(Vector3));
                float distance = ent.position.Distance(localPos);
                float distanceMeters = distance / 52.5f;
                
                char distText[32];
                snprintf(distText, sizeof(distText), "%.0fm", distanceMeters);
                ImVec2 textSize = ImGui::CalcTextSize(distText);
                ImVec2 textPos(feetScreen.x - textSize.x / 2.0f, feetScreen.y + 25);
                
                drawList->AddText(ImVec2(textPos.x - 1, textPos.y + 1), ImColor(0, 0, 0, 255), distText);
                drawList->AddText(ImVec2(textPos.x + 1, textPos.y - 1), ImColor(0, 0, 0, 255), distText);
                drawList->AddText(textPos, color, distText);
            }
        }
        
        // Draw entity name
        std::string displayName = ent.name;
        if (displayName.empty()) {
            displayName = isEnemy ? "ENEMY" : "TEAMMATE";
        }
        
        float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
        ImVec2 textPos(headScreen.x - textWidth / 2, boxPos.y - 25);
        
        drawList->AddText(ImVec2(textPos.x - 1, textPos.y), ImColor(0, 0, 0, 255), displayName.c_str());
        drawList->AddText(ImVec2(textPos.x + 1, textPos.y), ImColor(0, 0, 0, 255), displayName.c_str());
        drawList->AddText(ImVec2(textPos.x, textPos.y - 1), ImColor(0, 0, 0, 255), displayName.c_str());
        drawList->AddText(ImVec2(textPos.x, textPos.y + 1), ImColor(0, 0, 0, 255), displayName.c_str());
        drawList->AddText(textPos, color, displayName.c_str());
        
        // Draw health bar
        float hpPercent = (float)ent.health / (float)ent.max_health;
        if (hpPercent < 0.0f) hpPercent = 0.0f;
        if (hpPercent > 1.0f) hpPercent = 1.0f;
        
        float barWidth = boxSize.x;
        float barHeight = 6.0f;
        Vec2 barPos(boxPos.x, boxPos.y - 12);
        
        drawList->AddRectFilled(barPos.ToImVec2(), ImVec2(barPos.x + barWidth, barPos.y + barHeight), ImColor(0, 0, 0, 220), 2.0f);
        
        ImColor hpColor;
        if (hpPercent > 0.66f) hpColor = ImColor(0, 255, 0, 255);
        else if (hpPercent > 0.33f) hpColor = ImColor(255, 165, 0, 255);
        else hpColor = ImColor(255, 0, 0, 255);
        
        if (hpPercent > 0.01f) {
            drawList->AddRectFilled(barPos.ToImVec2(), ImVec2(barPos.x + barWidth * hpPercent, barPos.y + barHeight), hpColor, 2.0f);
        }
        
        drawList->AddRect(barPos.ToImVec2(), ImVec2(barPos.x + barWidth, barPos.y + barHeight), ImColor(150, 150, 150, 255), 2.0f, 0, 1.5f);
        
        char hpText[64];
        snprintf(hpText, sizeof(hpText), "HP: %d", ent.health);
        float hpTextWidth = ImGui::CalcTextSize(hpText).x;
        ImVec2 hpTextPos(headScreen.x - hpTextWidth / 2, feetScreen.y + 5);
        
        drawList->AddText(ImVec2(hpTextPos.x - 1, hpTextPos.y + 1), ImColor(0, 0, 0), hpText);
        drawList->AddText(ImVec2(hpTextPos.x + 1, hpTextPos.y - 1), ImColor(0, 0, 0), hpText);
        drawList->AddText(hpTextPos, ImColor(255, 255, 255, 255), hpText);
    }
};
