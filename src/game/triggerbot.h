#pragma once
#include "../driver/driver_interface.h"
#include "entity.h"
#include "../overlay/overlay.h"
#include "feature_manager.h"
#include <random>
#include <vector>

// Delay presets (5 levels)
enum class TriggerDelayLevel {
    LEVEL_1 = 0,  // 200-300ms (Very Safe)
    LEVEL_2 = 1,  // 100-200ms (Safe)
    LEVEL_3 = 2,  // 50-100ms (Balanced)
    LEVEL_4 = 3,  // 25-50ms (Aggressive)
    LEVEL_5 = 4   // 10-25ms (Very Aggressive)
};

// Bone selection modes
enum class TriggerBoneMode {
    BODY_ONLY = 0,     // Only body shots
    HEAD_ONLY = 1,     // Only head shots
    HEAD_BODY = 2,     // Head + Body (priority head)
    FULL_BODY = 3      // All bones (head, neck, chest, pelvis)
};

// Internal vector type for triggerbot (avoid conflicts with OS-ImGui's Vec3)
struct TriggerVec3 {
    float x, y, z;
};

class TriggerBot : public IFeature {
private:
    bool enabled;
    DWORD lastTriggerTime;
    DriverInterface* driver;
    Overlay* overlay;
    
    // Configuration
    TriggerDelayLevel delayLevel;
    TriggerBoneMode boneMode;
    bool showHitboxes;
    bool usePrediction;
    float extraPadding;
    float closeRangeThreshold;
    
    // Hotkey
    int triggerKey;
    bool isSelectingKey;
    
    // Randomization for human-like behavior
    std::random_device rd;
    std::mt19937 gen;
    
    // Dynamic delay based on level
    int GetRandomDelay();
    
    // Bone selection based on mode
    std::vector<int> GetActiveBones();
    
    // Bounding box calculation
    TriggerVec3 GetBoxDimensionsForBone(int boneIndex);
    std::vector<TriggerVec3> CalculateBoxCorners(const TriggerVec3& bonePos, const TriggerVec3& dimensions);
    
    // Prediction
    TriggerVec3 CalculatePredictedPosition(const TriggerVec3& bonePos, const TriggerVec3& velocity, float projectileSpeed);
    
    // Main detection logic
    bool ShouldTrigger();
    bool CheckBoneOnCrosshair(Entity& entity, int boneIndex);
    
public:
    TriggerBot(DriverInterface* drv, Overlay* ovl);
    ~TriggerBot();
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override;
    void RenderMenu();
    void RenderHitboxes();

    // IFeature implementation
    bool IsEnabled() const override { return enabled; }
    void SetEnabled(bool e) override { enabled = e; }
    const char* GetName() const override { return "TriggerBot"; }
};
