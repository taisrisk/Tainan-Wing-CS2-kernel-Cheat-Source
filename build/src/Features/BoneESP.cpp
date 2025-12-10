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
    
    // Get bone array address (modelState + 0x80)
    std::uintptr_t boneArrayAddress = drv.read<std::uintptr_t>(gameSceneNode + offsets.m_modelState);
    if (boneArrayAddress == 0) {
        return false;
    }
    
    boneArrayAddress += 0x80;
    
    // Clear existing data
    bonePositions.clear();
    IBoneData.clear();
    BonePosList.clear();
    bonePositions.reserve(NUM_BONES);
    IBoneData.reserve(NUM_BONES);
    BonePosList.reserve(NUM_BONES);
    
    // Get screen dimensions
    float screenWidth = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float screenHeight = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    
    // Read bone data with full rotation info (CBoneData)
    CBoneData boneData[NUM_BONES];
    if (!drv.read_memory(reinterpret_cast<void*>(boneArrayAddress), boneData, NUM_BONES * sizeof(CBoneData))) {
        return false;
    }
    
    // Read position-only data (BoneJointData) - for compatibility
    BoneJointData posData[NUM_BONES];
    if (!drv.read_memory(reinterpret_cast<void*>(boneArrayAddress), posData, NUM_BONES * sizeof(BoneJointData))) {
        return false;
    }
    
    // Process all bones
    for (size_t i = 0; i < NUM_BONES; ++i) {
        // Convert to screen space
        Vector3 screenPos;
        bool isVisible = WorldToScreen(posData[i].Pos, screenPos, viewMatrix, screenWidth, screenHeight);
        
        // Store position data
        bonePositions.push_back({ posData[i].Pos, screenPos, isVisible });
        BonePosList.push_back({ posData[i].Pos, screenPos, isVisible });
        
        // Store full bone data with rotation
        IBoneData.push_back({
            posData[i].Pos,
            posData[i].Scale,
            boneData[i].Rotation
        });
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
    connections.reserve(20); // Pre-allocate
    
    // Trunk connections (neck -> spine chain -> pelvis)
    connections.push_back({ neck_0, spine_3 });
    connections.push_back({ spine_3, spine_2 });
    connections.push_back({ spine_2, spine_1 });
    connections.push_back({ spine_1, spine_0 });
    connections.push_back({ spine_0, pelvis });
    
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
