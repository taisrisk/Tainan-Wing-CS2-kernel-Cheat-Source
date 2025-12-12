#pragma once
#include "../Helpers/OS-ImGui/OS-ImGui.h"
#include "entity.hpp"
#include "HeadAngleLine.hpp"
#include "BoneESP.hpp"
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
<<<<<<< Updated upstream
=======
    bool boneESPEnabled = false;
    bool teamCheckEnabled = true;
    BoneESP boneEsp;
>>>>>>> Stashed changes
    
    // Visibility cache for snaplines
    std::vector<bool> enemyVisibility;
    std::vector<bool> teammateVisibility;
    
public:
    ImGuiESP(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client), headAngleLine(driver, client), visibilityChecker(driver, client), boneEsp(driver, client) {
        lastFrameTime = std::chrono::steady_clock::now();
        headAngleLine.setScreenDimensions(screenWidth, screenHeight);
    }
    
    void updateEntities(const EntityManager& entities, std::uintptr_t localAddr) {
        int writeIdx = writeIndex.load(std::memory_order_relaxed);
        entityBuffer[writeIdx] = entities;
        localPlayerAddress = localAddr;
        
        // Update head angle line local player
        headAngleLine.setLocalPlayerPawn(localAddr);
        
        // Update visibility cache if checker is enabled (or snapline wall-check on)
        if ((visibilityChecker.isEnabled() || snaplinesWallCheckEnabled) && localAddr != 0) {
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
<<<<<<< Updated upstream
=======
    void setBoneESPEnabled(bool enable) { boneESPEnabled = enable; boneEsp.setEnabled(enable); }
    bool isBoneESPEnabled() const { return boneESPEnabled; }
    void setTeamCheckEnabled(bool enable) { teamCheckEnabled = enable; }
>>>>>>> Stashed changes
    
    void updateViewMatrix() {
        std::uintptr_t matrixAddress = clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix;
        drv.read_memory(reinterpret_cast<PVOID>(matrixAddress), viewMatrix, sizeof(viewMatrix));
    }
    
    void render() {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        // Always refresh view matrix for correct W2S across all features
        updateViewMatrix();

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
        
        // Render enemies with visibility index (always render enemies)
        for (size_t i = 0; i < currentEntities.enemy_entities.size(); i++) {
            const auto& ent = currentEntities.enemy_entities[i];
            if (!ent.is_valid || ent.address == localPlayerAddress) continue;
            
            bool visible = ((visibilityChecker.isEnabled() || snaplinesWallCheckEnabled) && i < enemyVisibility.size()) ? enemyVisibility[i] : true;
            drawEntity(ent, ImColor(255, 0, 0, 255), true, visible);
        }
        
        // Render teammates only when team check is OFF (i.e., show both teams)
        if (!teamCheckEnabled) {
            for (size_t i = 0; i < currentEntities.teammate_entities.size(); i++) {
                const auto& ent = currentEntities.teammate_entities[i];
                if (!ent.is_valid || ent.address == localPlayerAddress) continue;
                
                bool visible = ((visibilityChecker.isEnabled() || snaplinesWallCheckEnabled) && i < teammateVisibility.size()) ? teammateVisibility[i] : true;
                drawEntity(ent, ImColor(0, 0, 255, 255), false, visible);
            }
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

        // If visibility filtering is enabled and target not visible, skip drawing ESP elements
        if ((visibilityChecker.isEnabled() || snaplinesWallCheckEnabled) && !isVisible) {
            return;
        }
        
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

        // Bone ESP rendering (direct bone buffer, exact indices and connections)
        if (boneESPEnabled) {
            const auto& offsets = OffsetsManager::Get();
            std::uintptr_t gameScene = drv.read<std::uintptr_t>(ent.address + offsets.m_pGameSceneNode);
            if (gameScene != 0) {
                std::uintptr_t boneIndex = drv.read<std::uintptr_t>(gameScene + offsets.m_modelState + 0x80);
                if (boneIndex != 0) {
                    enum BoneIndex {
                        HEAD = 6, NECK = 5, SPINE = 4, PELVIS = 0,
                        L_SHOULDER = 8, L_ARM = 9, L_HAND = 11,
                        R_SHOULDER = 13, R_ARM = 14, R_HAND = 16,
                        SPINE1 = 2,
                        L_HIP = 22, L_KNEE = 23, L_FOOT = 24,
                        R_HIP = 25, R_KNEE = 26, R_FOOT = 27
                    };

                    static const std::pair<int, int> connections[] = {
                        { HEAD, NECK }, { NECK, SPINE }, { SPINE, PELVIS },
                        { SPINE, L_SHOULDER }, { L_SHOULDER, L_ARM }, { L_ARM, L_HAND },
                        { SPINE, R_SHOULDER }, { R_SHOULDER, R_ARM }, { R_ARM, R_HAND },
                        { SPINE, SPINE1 }, { PELVIS, L_HIP }, { PELVIS, R_HIP },
                        { L_HIP, L_KNEE }, { L_KNEE, L_FOOT },
                        { R_HIP, R_KNEE }, { R_KNEE, R_FOOT }
                    };

                    // Draw skeleton connections
                    for (const auto& conn : connections) {
                        int b1 = conn.first;
                        int b2 = conn.second;
                        Vector3 v1 = drv.read<Vector3>(boneIndex + static_cast<std::uintptr_t>(b1) * 32);
                        Vector3 v2 = drv.read<Vector3>(boneIndex + static_cast<std::uintptr_t>(b2) * 32);
                        Vec2 s1{}, s2{};
                        if (worldToScreen(v1, s1) && worldToScreen(v2, s2)) {
                            drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), color, 1.0f);
                        }
                    }

                    // China hat on top of head (screen-proportional, distance-stable)
                    {
                        Vector3 vHead = drv.read<Vector3>(boneIndex + static_cast<std::uintptr_t>(HEAD) * 32);
                        Vec2 sHead{};
                        if (worldToScreen(vHead, sHead)) {
                            // Scale relative to on-screen box height with lower mins for far targets
                            float h = std::clamp(height, 0.0f, 1000.0f);
                            if (h >= 8.0f) {
                                float brimRadius = std::clamp(h * 0.16f, 4.0f, 18.0f);
                                float apexOffset = std::clamp(h * 0.18f, 6.0f, 24.0f);
                                // Keep brim near the top of the head (slightly above)
                                float brimYOffsetTop = std::clamp(h * 0.01f, 1.0f, 6.0f);

                                // Flatter brim at distance (smaller h), less flatten near
                                float hNorm = std::clamp(h / 200.0f, 0.0f, 1.0f);
                                float brimFlatten = 0.22f + 0.28f * hNorm; // 0.22 (far) .. 0.50 (near)

                                const int segments = 26;
                                const int spokeCount = 12;

                                float baseLift = std::clamp(h * 0.05f, 3.0f, 14.0f);
                                ImVec2 apex(sHead.x, sHead.y - (apexOffset + baseLift));
                                ImVec2 brimCenter(sHead.x, sHead.y - brimYOffsetTop);

                                float alphaFactor = std::clamp(h / 200.0f, 0.35f, 1.0f);
                                ImColor spokeCol(color.Value.x, color.Value.y, color.Value.z, 0.70f * alphaFactor);
                                ImColor rimCol  (color.Value.x, color.Value.y, color.Value.z, 0.90f * alphaFactor);
                                ImColor shadeCol(0.0f, 0.0f, 0.0f, 0.25f);

                                // Brim ellipse
                                ImVec2 firstPt{}, prevPt{};
                                for (int i = 0; i < segments; ++i) {
                                    float t = (2.0f * 3.14159265f) * (static_cast<float>(i) / segments);
                                    ImVec2 pt(
                                        brimCenter.x + brimRadius * cosf(t),
                                        brimCenter.y + (brimRadius * brimFlatten) * sinf(t)
                                    );
                                    if (i == 0) firstPt = pt; else {
                                        drawList->AddLine(ImVec2(prevPt.x, prevPt.y + 1.0f), ImVec2(pt.x, pt.y + 1.0f), shadeCol, 1.0f);
                                        drawList->AddLine(prevPt, pt, rimCol, 1.8f);
                                    }
                                    prevPt = pt;
                                }
                                // Close ring
                                drawList->AddLine(ImVec2(prevPt.x, prevPt.y + 1.0f), ImVec2(firstPt.x, firstPt.y + 1.0f), shadeCol, 1.0f);
                                drawList->AddLine(prevPt, firstPt, rimCol, 1.8f);

                                // Spokes
                                for (int i = 0; i < spokeCount; ++i) {
                                    float t = (2.0f * 3.14159265f) * (static_cast<float>(i) / spokeCount);
                                    ImVec2 brimPt(
                                        brimCenter.x + brimRadius * cosf(t),
                                        brimCenter.y + (brimRadius * brimFlatten) * sinf(t)
                                    );
                                    drawList->AddLine(ImVec2(apex.x, apex.y + 1.0f), ImVec2(brimPt.x, brimPt.y + 1.0f), shadeCol, 1.0f);
                                    drawList->AddLine(apex, brimPt, spokeCol, 1.2f);
                                }
                            }
                        }
                    }
                }
            }
        }
        
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
