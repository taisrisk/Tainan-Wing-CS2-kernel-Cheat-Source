#pragma once

#include "../Core/driver.hpp"
#include "../Offsets/offsets.hpp"
#include "../Offsets/offset_manager.hpp"
#include <cmath>
#include <chrono>

class RecoilCompensation {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    
    struct RecoilState {
        float pitch = 0.0f;  // Vertical recoil
        float yaw = 0.0f;    // Horizontal recoil
        int shotsFired = 0;
        bool isRecovering = false;
        std::chrono::steady_clock::time_point lastShotTime;
    };
    
    RecoilState currentRecoil;
    const float RECOIL_RECOVERY_TIME_MS = 150.0f;  // Time for recoil to reset

public:
    RecoilCompensation(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client) {
        currentRecoil.lastShotTime = std::chrono::steady_clock::now();
    }
    
    // Read current recoil punch from local player
    void updateRecoil(std::uintptr_t localPlayerPawn) {
        if (localPlayerPawn == 0) {
            resetRecoil();
            return;
        }
        
        const auto& offsets = OffsetsManager::Get();
        
        // Read shots fired counter
        int32_t shotsFired = drv.read<int32_t>(localPlayerPawn + offsets.m_iShotsFired);
        
        // If shots fired changed, update recoil state
        if (shotsFired != currentRecoil.shotsFired) {
            currentRecoil.shotsFired = shotsFired;
            currentRecoil.lastShotTime = std::chrono::steady_clock::now();
            currentRecoil.isRecovering = false;
        }
        
        // Check if recoil should be resetting
        auto now = std::chrono::steady_clock::now();
        float timeSinceShot = std::chrono::duration<float, std::milli>(now - currentRecoil.lastShotTime).count();
        
        if (timeSinceShot > RECOIL_RECOVERY_TIME_MS) {
            // Recoil has fully recovered
            resetRecoil();
        } else {
            // Read current recoil punch angles
            Vector3 aimPunch;
            drv.read_memory(reinterpret_cast<void*>(localPlayerPawn + offsets.m_aimPunchAngle), &aimPunch, sizeof(Vector3));
            
            currentRecoil.pitch = aimPunch.x;
            currentRecoil.yaw = aimPunch.y;
        }
    }
    
    // Check if weapon is currently in recoil recovery
    bool isInRecoil() const {
        return currentRecoil.shotsFired > 0 || 
               std::abs(currentRecoil.pitch) > 0.1f || 
               std::abs(currentRecoil.yaw) > 0.1f;
    }
    
    // Get recoil compensation angles (inverse of current recoil)
    void getCompensation(float& outPitch, float& outYaw) const {
        outPitch = -currentRecoil.pitch;
        outYaw = -currentRecoil.yaw;
    }
    
    // Get current recoil magnitude
    float getRecoilMagnitude() const {
        return std::sqrt(currentRecoil.pitch * currentRecoil.pitch + 
                        currentRecoil.yaw * currentRecoil.yaw);
    }
    
    // Reset recoil state
    void resetRecoil() {
        currentRecoil.pitch = 0.0f;
        currentRecoil.yaw = 0.0f;
        currentRecoil.shotsFired = 0;
        currentRecoil.isRecovering = false;
    }
    
    // Get shots fired count
    int getShotsFired() const {
        return currentRecoil.shotsFired;
    }
};
