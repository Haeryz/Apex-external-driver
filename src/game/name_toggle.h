#pragma once
#include "feature_manager.h"

// Name Toggle Manager - Show/Hide player names
class NameToggleManager : public IFeature {
private:
    bool showNames = true; // Default: show names
    
public:
    NameToggleManager() = default;
    
    // IFeature implementation
    const char* GetName() const override { return "Names"; }
    bool IsEnabled() const override { return showNames; }
    void SetEnabled(bool enabled) override { showNames = enabled; }
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override {
        // No per-frame update needed
    }
    
    // Toggle names on/off
    void Toggle() {
        showNames = !showNames;
    }
    
    // Check if names should be shown
    bool ShouldShowNames() const {
        return showNames;
    }
};
