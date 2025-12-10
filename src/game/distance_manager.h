#pragma once
#include "feature_manager.h"

// Distance Manager - Custom distance control
class DistanceManager : public IFeature {
private:
    float maxDistance = 500.0f; // Custom distance in meters (100-1000)
    
public:
    DistanceManager() = default;
    
    // IFeature implementation
    const char* GetName() const override { return "Distance"; }
    bool IsEnabled() const override { return true; } // Always enabled
    void SetEnabled(bool enabled) override { /* Always on */ }
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override {
        // No per-frame update needed
    }
    
    // Toggle to next distance preset (for legacy support)
    void CycleDistance() {
        if (maxDistance == 250.0f) maxDistance = 500.0f;
        else if (maxDistance == 500.0f) maxDistance = 1000.0f;
        else maxDistance = 250.0f;
    }
    
    // Set custom max distance (100-1000m)
    void SetMaxDistance(float distance) {
        if (distance >= 100.0f && distance <= 1000.0f) {
            maxDistance = distance;
        }
    }
    
    // Get current max distance
    float GetMaxDistance() const {
        return maxDistance;
    }
    
    // Get current distance as string (for display)
    const char* GetDistanceText() const {
        static char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.0fm", maxDistance);
        return buffer;
    }
};
