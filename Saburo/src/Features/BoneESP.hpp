#pragma once

#include <vector>
#include <cmath>
#include "../Core/driver.hpp"
#include "../Offsets/offset_manager.hpp"
#include "entity.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// CS2 Bone Indices
enum BONEINDEX : uint32_t {
    head = 6,
    neck_0 = 5,
    spine_1 = 2,
    spine_2 = 3,
    pelvis = 0,
    arm_upper_L = 8,
    arm_lower_L = 9,
    hand_L = 10,
    arm_upper_R = 13,
    arm_lower_R = 14,
    hand_R = 15,
    leg_upper_L = 22,
    leg_lower_L = 23,
    ankle_L = 24,
    leg_upper_R = 25,
    leg_lower_R = 26,
    ankle_R = 27,
};

// Simplified bone data structure for kernel read
struct BoneJointData {
    Vector3 Pos;
    float Scale;
};

struct BoneJointPos {
    Vector3 Pos;
    Vector3 ScreenPos;  // Using Vector3 for consistency, z will be depth
    bool IsVisible;
};

class BoneESP {
private:
    driver::DriverHandle& drv;
    std::uintptr_t clientBase;
    
    // Bone connection lists
    static constexpr uint32_t Trunk[] = { neck_0, spine_2, spine_1, pelvis };
    static constexpr uint32_t LeftArm[] = { neck_0, arm_upper_L, arm_lower_L, hand_L };
    static constexpr uint32_t RightArm[] = { neck_0, arm_upper_R, arm_lower_R, hand_R };
    static constexpr uint32_t LeftLeg[] = { pelvis, leg_upper_L, leg_lower_L, ankle_L };
    static constexpr uint32_t RightLeg[] = { pelvis, leg_upper_R, leg_lower_R, ankle_R };
    
    static constexpr size_t NUM_BONES = 30;
    static constexpr size_t BONE_STRIDE = 32; // Distance between bones in memory
    
    bool enabled = false;

public:
    BoneESP(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client) {}
    
    void setEnabled(bool enable) {
        enabled = enable;
    }
    
    bool isEnabled() const {
        return enabled;
    }
    
    // Read all bone data for an entity
    bool UpdateBoneData(std::uintptr_t entityAddress, std::vector<BoneJointPos>& bonePositions, const float* viewMatrix);
    
    // Get bone connections for rendering
    struct BoneConnection {
        uint32_t fromBone;
        uint32_t toBone;
    };
    
    static std::vector<BoneConnection> GetBoneConnections();
    
private:
    bool WorldToScreen(const Vector3& world, Vector3& screen, const float* viewMatrix, float screenWidth, float screenHeight);
};
