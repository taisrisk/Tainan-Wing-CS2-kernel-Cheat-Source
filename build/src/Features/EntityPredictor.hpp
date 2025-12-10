#pragma once

#include "entity.hpp"
#include "../Core/driver.hpp"
#include "../Offsets/offset_manager.hpp"
#include <chrono>
#include <cmath>
#include <unordered_map>

class EntityPredictor {
private:
    struct PredictionData {
        Vector3 lastPosition;
        Vector3 velocity;
        std::chrono::steady_clock::time_point lastUpdate;
        bool hasHistory = false;
    };
    
    std::unordered_map<std::uintptr_t, PredictionData> entityHistory;
    driver::DriverHandle& drv;
    
public:
    EntityPredictor(driver::DriverHandle& driver) : drv(driver) {}
    
    // Update entity velocity tracking
    void updateEntity(std::uintptr_t entityAddress, const Vector3& currentPosition) {
        auto now = std::chrono::steady_clock::now();
        auto& data = entityHistory[entityAddress];
        
        if (data.hasHistory) {
            // Calculate time delta
            float deltaTime = std::chrono::duration<float>(now - data.lastUpdate).count();
            
            if (deltaTime > 0.001f && deltaTime < 0.5f) {  // Ignore huge jumps
                // Calculate velocity
                data.velocity.x = (currentPosition.x - data.lastPosition.x) / deltaTime;
                data.velocity.y = (currentPosition.y - data.lastPosition.y) / deltaTime;
                data.velocity.z = (currentPosition.z - data.lastPosition.z) / deltaTime;
            }
        } else {
            data.hasHistory = true;
            data.velocity = {0, 0, 0};
        }
        
        data.lastPosition = currentPosition;
        data.lastUpdate = now;
    }
    
    // Predict entity position based on velocity and time
    Vector3 predictPosition(std::uintptr_t entityAddress, float predictionTimeMs) const {
        auto it = entityHistory.find(entityAddress);
        if (it == entityHistory.end() || !it->second.hasHistory) {
            // No history, return zero position
            return {0, 0, 0};
        }
        
        const auto& data = it->second;
        float predictionTime = predictionTimeMs / 1000.0f;  // Convert to seconds
        
        // Simple linear prediction: pos = current + velocity * time
        Vector3 predicted;
        predicted.x = data.lastPosition.x + data.velocity.x * predictionTime;
        predicted.y = data.lastPosition.y + data.velocity.y * predictionTime;
        predicted.z = data.lastPosition.z + data.velocity.z * predictionTime;
        
        return predicted;
    }
    
    // Get entity's current velocity
    Vector3 getVelocity(std::uintptr_t entityAddress) const {
        auto it = entityHistory.find(entityAddress);
        if (it == entityHistory.end() || !it->second.hasHistory) {
            return {0, 0, 0};
        }
        return it->second.velocity;
    }
    
    // Check if entity is moving
    bool isMoving(std::uintptr_t entityAddress, float threshold = 10.0f) const {
        Vector3 vel = getVelocity(entityAddress);
        float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        return speed > threshold;
    }
    
    // Clear old entity data
    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = entityHistory.begin(); it != entityHistory.end();) {
            float timeSinceUpdate = std::chrono::duration<float>(now - it->second.lastUpdate).count();
            if (timeSinceUpdate > 1.0f) {  // Remove entities not updated in 1 second
                it = entityHistory.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Clear all prediction data
    void reset() {
        entityHistory.clear();
    }
};
