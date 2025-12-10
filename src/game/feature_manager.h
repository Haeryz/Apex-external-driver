#pragma once
#include <Windows.h>
#include <map>
#include <string>
#include <chrono>

// Base interface for all features
class IFeature {
public:
    virtual ~IFeature() = default;
    virtual void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual const char* GetName() const = 0;
};

// Feature manager to handle all game features
class FeatureManager {
private:
    std::map<int, IFeature*> featureBindings;  // Hotkey ID -> Feature
    std::map<std::string, IFeature*> features;  // Name -> Feature
    std::map<int, int> hotkeyIdMap;            // VK_KEY -> Hotkey ID
    
    HWND targetWindow = nullptr;
    int nextHotkeyId = 1;
    
public:
    FeatureManager() = default;
    ~FeatureManager() {
        // Unregister all hotkeys
        if (targetWindow) {
            for (auto& [vkKey, hotkeyId] : hotkeyIdMap) {
                UnregisterHotKey(targetWindow, hotkeyId);
            }
        }
        
        // Features are managed externally, just clear pointers
        features.clear();
        featureBindings.clear();
        hotkeyIdMap.clear();
    }
    
    // Set the window handle for hotkey registration (call from overlay thread)
    void SetTargetWindow(HWND hwnd) {
        targetWindow = hwnd;
    }
    
    // Register a feature with a hotkey binding (using RegisterHotKey)
    void RegisterFeature(const char* name, IFeature* feature, int vkKey) {
        features[name] = feature;
        
        // Register global hotkey if window handle available
        if (targetWindow) {
            int hotkeyId = nextHotkeyId++;
            
            // MOD_NOREPEAT prevents repeated WM_HOTKEY messages when key is held
            if (RegisterHotKey(targetWindow, hotkeyId, MOD_NOREPEAT, vkKey)) {
                featureBindings[hotkeyId] = feature;
                hotkeyIdMap[vkKey] = hotkeyId;
            }
        }
    }
    
    // Process hotkey message (call from WndProc with WM_HOTKEY)
    void ProcessHotkeyMessage(int hotkeyId) {
        auto it = featureBindings.find(hotkeyId);
        if (it != featureBindings.end()) {
            IFeature* feature = it->second;
            bool currentState = feature->IsEnabled();
            feature->SetEnabled(!currentState);
        }
    }
    
    // Legacy method (not used with RegisterHotKey)
    void ProcessHotkeys() {
        // No longer needed - hotkeys handled via WM_HOTKEY messages
    }
    
    // Update all enabled features
    void UpdateAllFeatures(uintptr_t localPlayerPtr, uintptr_t gameBase) {
        for (auto& [name, feature] : features) {
            if (feature->IsEnabled()) {
                feature->Update(localPlayerPtr, gameBase);
            }
        }
    }
    
    // Get feature by name
    IFeature* GetFeature(const char* name) {
        auto it = features.find(name);
        return (it != features.end()) ? it->second : nullptr;
    }
    
    // Get all features (for overlay display)
    const std::map<std::string, IFeature*>& GetAllFeatures() const {
        return features;
    }
};
