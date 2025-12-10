#pragma once

#include <vector>
#include <cmath>
#include <optional>
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
    spine_0 = 1,
    spine_1 = 2,
    spine_2 = 3,
    spine_3 = 4,
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

// Quaternion for bone rotation
class Quaternion_t {
public:
    float x, y, z, w;

    Quaternion_t() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion_t(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static Quaternion_t Identity() {
        return Quaternion_t(0.0f, 0.0f, 0.0f, 1.0f);
    }

    void Normalize() {
        float length = sqrtf(x * x + y * y + z * z + w * w);
        if (length > 0.0f) {
            x /= length; y /= length; z /= length; w /= length;
        }
    }

    bool IsValid() const {
        return !isnan(x) && !isnan(y) && !isnan(z) && !isnan(w) &&
               !isinf(x) && !isinf(y) && !isinf(z) && !isinf(w);
    }
};

// Full bone data with rotation (32 bytes per bone)
struct CBoneData {
    Vector3 Location;
    float Scale;
    Quaternion_t Rotation;
};

// Simplified bone data for position only (16 bytes per bone)
struct BoneJointData {
    Vector3 Pos;
    float Scale;
};

// Screen-space bone position
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
    static constexpr uint32_t Trunk[] = { neck_0, spine_3, spine_2, spine_1, spine_0, pelvis };
    static constexpr uint32_t LeftArm[] = { neck_0, arm_upper_L, arm_lower_L, hand_L };
    static constexpr uint32_t RightArm[] = { neck_0, arm_upper_R, arm_lower_R, hand_R };
    static constexpr uint32_t LeftLeg[] = { pelvis, leg_upper_L, leg_lower_L, ankle_L };
    static constexpr uint32_t RightLeg[] = { pelvis, leg_upper_R, leg_lower_R, ankle_R };
    
    static constexpr size_t NUM_BONES = 30;
    static constexpr size_t BONE_STRIDE = 32; // 32 bytes between bones (CBoneData size)
    
    bool enabled = false;

public:
    std::vector<BoneJointPos> BonePosList;
    std::vector<CBoneData> IBoneData;

    BoneESP(driver::DriverHandle& driver, std::uintptr_t client)
        : drv(driver), clientBase(client) {}
    
    void setEnabled(bool enable) {
        enabled = enable;
    }
    
    bool isEnabled() const {
        return enabled;
    }
    
    // Read all bone data for an entity (OPTIMIZED)
    bool UpdateBoneData(std::uintptr_t entityAddress, std::vector<BoneJointPos>& bonePositions, const float* viewMatrix);
    
    // Get bone data with rotation
    std::optional<CBoneData> GetBoneData(int index) const {
        if (index < 0 || index >= static_cast<int>(IBoneData.size())) {
            return std::nullopt;
        }
        return IBoneData[index];
    }
    
    // Get bone connections for rendering
    struct BoneConnection {
        uint32_t fromBone;
        uint32_t toBone;
    };
    
    static std::vector<BoneConnection> GetBoneConnections();
    
private:
    bool WorldToScreen(const Vector3& world, Vector3& screen, const float* viewMatrix, float screenWidth, float screenHeight);
};
