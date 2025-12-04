#include "BoneESP.hpp"
#include <algorithm>

bool BoneESP::UpdateBoneData(std::uintptr_t entityAddress, std::vector<BoneJointPos>& bonePositions, const float* viewMatrix) {
    if (!enabled || entityAddress == 0) {
        return false;
    }
    
    const auto& offsets = OffsetsManager::Get();
    
    // Get game scene node
    std::uintptr_t gameSceneNode = drv.read<std::uintptr_t>(entityAddress + offsets.m_pGameSceneNode);
    if (gameSceneNode == 0) {
        return false;
    }
    
    // Get bone array address
    // modelState offset + 0x80 gives us bone array
    std::uintptr_t boneArrayAddress = drv.read<std::uintptr_t>(gameSceneNode + offsets.m_modelState);
    if (boneArrayAddress == 0) {
        return false;
    }
    
    // Add 0x80 offset to get actual bone data
    boneArrayAddress += 0x80;
    
    // Clear existing bone data
    bonePositions.clear();
    bonePositions.reserve(NUM_BONES);
    
    // Get screen dimensions
    float screenWidth = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float screenHeight = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    
    // Read all bones
    for (size_t i = 0; i < NUM_BONES; ++i) {
        BoneJointData boneData;
        std::uintptr_t boneAddress = boneArrayAddress + (i * BONE_STRIDE);
        
        // Read bone position (first 12 bytes are x, y, z)
        if (!drv.read_memory(reinterpret_cast<void*>(boneAddress), &boneData.Pos, sizeof(Vector3))) {
            return false;
        }
        
        // Convert to screen space
        Vector3 screenPos;
        bool isVisible = WorldToScreen(boneData.Pos, screenPos, viewMatrix, screenWidth, screenHeight);
        
        bonePositions.push_back({ boneData.Pos, screenPos, isVisible });
    }
    
    return !bonePositions.empty();
}

bool BoneESP::WorldToScreen(const Vector3& world, Vector3& screen, const float* viewMatrix, float screenWidth, float screenHeight) {
    float w = viewMatrix[12] * world.x + viewMatrix[13] * world.y +
        viewMatrix[14] * world.z + viewMatrix[15];
    
    if (w < 0.01f) {
        return false;
    }
    
    float x = viewMatrix[0] * world.x + viewMatrix[1] * world.y +
        viewMatrix[2] * world.z + viewMatrix[3];
    float y = viewMatrix[4] * world.x + viewMatrix[5] * world.y +
        viewMatrix[6] * world.z + viewMatrix[7];
    
    x /= w;
    y /= w;
    
    screen.x = (screenWidth / 2.0f) * (1.0f + x);
    screen.y = (screenHeight / 2.0f) * (1.0f - y);
    screen.z = w; // Store depth
    
    bool valid = (screen.x >= 0 && screen.x <= screenWidth &&
                  screen.y >= 0 && screen.y <= screenHeight);
    
    return valid;
}

std::vector<BoneESP::BoneConnection> BoneESP::GetBoneConnections() {
    std::vector<BoneConnection> connections;
    
    // Trunk connections (neck -> spine -> pelvis)
    connections.push_back({ neck_0, spine_2 });
    connections.push_back({ spine_2, spine_1 });
    connections.push_back({ spine_1, pelvis });
    
    // Head connection
    connections.push_back({ head, neck_0 });
    
    // Left arm (neck -> upper -> lower -> hand)
    connections.push_back({ neck_0, arm_upper_L });
    connections.push_back({ arm_upper_L, arm_lower_L });
    connections.push_back({ arm_lower_L, hand_L });
    
    // Right arm
    connections.push_back({ neck_0, arm_upper_R });
    connections.push_back({ arm_upper_R, arm_lower_R });
    connections.push_back({ arm_lower_R, hand_R });
    
    // Left leg (pelvis -> upper -> lower -> ankle)
    connections.push_back({ pelvis, leg_upper_L });
    connections.push_back({ leg_upper_L, leg_lower_L });
    connections.push_back({ leg_lower_L, ankle_L });
    
    // Right leg
    connections.push_back({ pelvis, leg_upper_R });
    connections.push_back({ leg_upper_R, leg_lower_R });
    connections.push_back({ leg_lower_R, ankle_R });
    
    return connections;
}
