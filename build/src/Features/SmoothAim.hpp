#pragma once

#include "../Helpers/kernel_mouse.hpp"
#include <cmath>
#include <algorithm>

class SmoothAim {
private:
    float currentSmoothness;  // Higher = smoother (1.0 = instant, 10.0 = very smooth)
    const float MIN_SMOOTHNESS = 1.0f;
    const float MAX_SMOOTHNESS = 20.0f;
    
    struct AimTarget {
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        bool hasTarget = false;
    };
    
    AimTarget lastTarget;
    
public:
    SmoothAim(float smoothness = 5.0f) 
        : currentSmoothness(std::clamp(smoothness, MIN_SMOOTHNESS, MAX_SMOOTHNESS)) {}
    
    // Set smoothness factor
    void setSmoothness(float smoothness) {
        currentSmoothness = std::clamp(smoothness, MIN_SMOOTHNESS, MAX_SMOOTHNESS);
    }
    
    float getSmoothness() const {
        return currentSmoothness;
    }
    
    // Calculate smooth camera movement towards target
    // Returns the delta to apply this frame
    void calculateSmoothDelta(float targetDeltaX, float targetDeltaY, float& outDeltaX, float& outDeltaY) {
        // Apply smoothing factor - divide by smoothness to slow down movement
        outDeltaX = targetDeltaX / currentSmoothness;
        outDeltaY = targetDeltaY / currentSmoothness;
        
        // Clamp to prevent overshoot on small movements
        const float MIN_MOVE = 0.1f;
        if (std::abs(outDeltaX) < MIN_MOVE && std::abs(targetDeltaX) < MIN_MOVE) {
            outDeltaX = 0.0f;
        }
        if (std::abs(outDeltaY) < MIN_MOVE && std::abs(targetDeltaY) < MIN_MOVE) {
            outDeltaY = 0.0f;
        }
        
        lastTarget.deltaX = targetDeltaX;
        lastTarget.deltaY = targetDeltaY;
        lastTarget.hasTarget = true;
    }
    
    // Apply smooth camera movement
    void applySmooth(float targetDeltaX, float targetDeltaY) {
        float smoothDeltaX, smoothDeltaY;
        calculateSmoothDelta(targetDeltaX, targetDeltaY, smoothDeltaX, smoothDeltaY);
        
        if (smoothDeltaX != 0.0f || smoothDeltaY != 0.0f) {
            KernelMouse::MoveMouse(static_cast<LONG>(smoothDeltaX), static_cast<LONG>(smoothDeltaY));
        }
    }
    
    // Reset aim state
    void reset() {
        lastTarget.hasTarget = false;
        lastTarget.deltaX = 0.0f;
        lastTarget.deltaY = 0.0f;
    }
    
    // Check if we're close to target
    bool isOnTarget(float targetDeltaX, float targetDeltaY, float threshold = 5.0f) const {
        float distance = std::sqrt(targetDeltaX * targetDeltaX + targetDeltaY * targetDeltaY);
        return distance < threshold;
    }
};
