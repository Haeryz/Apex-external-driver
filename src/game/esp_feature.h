#pragma once
#include "feature_manager.h"
#include "esp_config.h"
#include "../memory/memory_reader.h"
#include "../memory/offsets.h"
#include "../OS-ImGui/imgui/imgui.h"
#include "entity.h"
#include "math.h"

class ESPFeature : public IFeature {
public:
    ESPConfig config;

    ESPFeature() = default;
    
    // IFeature interface implementation
    const char* GetName() const override { return "ESP"; }
    bool IsEnabled() const override { return config.enabled; }
    void SetEnabled(bool enabled) override { config.enabled = enabled; }
    
    // Update is not used for ESP (rendering is done in RenderESP())
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override {
        // ESP doesn't need update in memory loop
        // Rendering is done in the overlay thread
    }
    
    // Advanced rendering functions (DMA-style)
    void DrawHealthAndShieldBar(ImDrawList* drawList, const ImVec2& topLeft, const ImVec2& bottomRight, 
                                 int health, int maxHealth, int shield, int maxShield, int armorType) {
        float barWidth = 3.0f;
        float totalHeight = bottomRight.y - topLeft.y;
        float barX = topLeft.x - barWidth - 2.0f;
        
        // Health bar (left side)
        if (maxHealth > 0) {
            float healthPct = std::clamp((float)health / (float)maxHealth, 0.0f, 1.0f);
            float healthHeight = totalHeight * healthPct;
            
            // Background
            drawList->AddRectFilled(
                ImVec2(barX, topLeft.y),
                ImVec2(barX + barWidth, bottomRight.y),
                IM_COL32(0, 0, 0, 150)
            );
            
            // Health bar (green to red gradient)
            ImU32 healthColor = IM_COL32(
                static_cast<int>((1.0f - healthPct) * 255),
                static_cast<int>(healthPct * 255),
                0, 255
            );
            
            drawList->AddRectFilled(
                ImVec2(barX, bottomRight.y - healthHeight),
                ImVec2(barX + barWidth, bottomRight.y),
                healthColor
            );
        }
        
        // Shield bar (right side)
        if (maxShield > 0 && armorType > 0) {
            float shieldBarX = bottomRight.x + 2.0f;
            float shieldPct = std::clamp((float)shield / (float)maxShield, 0.0f, 1.0f);
            float shieldHeight = totalHeight * shieldPct;
            
            // Background
            drawList->AddRectFilled(
                ImVec2(shieldBarX, topLeft.y),
                ImVec2(shieldBarX + barWidth, bottomRight.y),
                IM_COL32(0, 0, 0, 150)
            );
            
            // Shield bar with armor color
            ImVec4 shieldColorVec = config.shieldColors[armorType];
            ImU32 shieldColor = IM_COL32(
                static_cast<int>(shieldColorVec.x * 255),
                static_cast<int>(shieldColorVec.y * 255),
                static_cast<int>(shieldColorVec.z * 255),
                255
            );
            
            drawList->AddRectFilled(
                ImVec2(shieldBarX, bottomRight.y - shieldHeight),
                ImVec2(shieldBarX + barWidth, bottomRight.y),
                shieldColor
            );
        }
    }
    
    void DrawPlayerInfo(ImDrawList* drawList, const ImVec2& pos, const Entity& entity, float distance, bool showNames = true) {
        ImVec2 textPos = pos;
        float lineHeight = 14.0f;
        
        // Distance only
        if (config.showDistances) {
            char distStr[32];
            sprintf_s(distStr, "%.0fm", distance);
            drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), distStr);
            textPos.y += lineHeight;
        }
        
        // Knocked state - only show within 200m
        if (entity.isKnocked && distance <= 200.0f) {
            drawList->AddText(textPos, IM_COL32(255, 0, 0, 255), "KNOCKED");
        }
    }
};
