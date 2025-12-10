#pragma once
#include "../OS-ImGui/imgui/imgui.h"
#include "../game/entity.h"
#include "../game/math.h"
#include <vector>

class ESPRenderer {
public:
    ESPRenderer(int width, int height) : screenWidth(width), screenHeight(height) {}
    
    void RenderESP(const std::vector<Entity>& entities, const Vector3& localPos, const Matrix& viewMatrix);
    
private:
    int screenWidth;
    int screenHeight;
    
    // Drawing functions
    void DrawBox(const Vector2& head, const Vector2& origin, ImU32 color);
    void DrawHealthBar(const Vector2& head, const Vector2& origin, int health, int maxHealth);
    void DrawShieldBar(const Vector2& head, const Vector2& origin, int shield, int maxShield, bool hasHealthBar);
    void DrawDistance(const Vector2& position, float distance);
    void DrawCorneredBox(float x, float y, float w, float h, ImU32 color, float thickness);
};

inline void ESPRenderer::DrawCorneredBox(float x, float y, float w, float h, ImU32 color, float thickness) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    
    float lineW = w / 3;
    float lineH = h / 3;
    
    // Top left
    draw->AddLine(ImVec2(x, y), ImVec2(x + lineW, y), color, thickness);
    draw->AddLine(ImVec2(x, y), ImVec2(x, y + lineH), color, thickness);
    
    // Top right
    draw->AddLine(ImVec2(x + w, y), ImVec2(x + w - lineW, y), color, thickness);
    draw->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + lineH), color, thickness);
    
    // Bottom left
    draw->AddLine(ImVec2(x, y + h), ImVec2(x + lineW, y + h), color, thickness);
    draw->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - lineH), color, thickness);
    
    // Bottom right
    draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - lineW, y + h), color, thickness);
    draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - lineH), color, thickness);
}

inline void ESPRenderer::DrawBox(const Vector2& head, const Vector2& origin, ImU32 color) {
    float height = abs(origin.y - head.y);
    float width = height * 0.5f;
    float x = head.x - (width / 2.0f);
    float y = head.y;
    
    DrawCorneredBox(x, y, width, height, color, 1.5f);
}

inline void ESPRenderer::DrawHealthBar(const Vector2& head, const Vector2& origin, int health, int maxHealth) {
    float height = abs(origin.y - head.y);
    float width = height * 0.5f;
    float x = head.x - (width / 2.0f);
    float y = head.y;
    
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    
    float barHeight = (float)health / (float)maxHealth * height;
    float barY = y + (height - barHeight);
    
    // Background
    draw->AddRectFilled(
        ImVec2(x - 6, y),
        ImVec2(x - 4, y + height),
        IM_COL32(20, 20, 20, 255)
    );
    
    // Health bar
    draw->AddRectFilled(
        ImVec2(x - 6, barY),
        ImVec2(x - 4, y + height),
        IM_COL32(0, 255, 0, 255)
    );
}

inline void ESPRenderer::DrawShieldBar(const Vector2& head, const Vector2& origin, int shield, int maxShield, bool hasHealthBar) {
    if (maxShield <= 0) return;
    
    float height = abs(origin.y - head.y);
    float width = height * 0.5f;
    float x = head.x - (width / 2.0f);
    float y = head.y;
    
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    
    float barHeight = (float)shield / (float)maxShield * height;
    float barY = y + (height - barHeight);
    float barX = hasHealthBar ? x - 8 : x - 4;
    
    // Background
    draw->AddRectFilled(
        ImVec2(barX - 2, y),
        ImVec2(barX, y + height),
        IM_COL32(20, 20, 20, 255)
    );
    
    // Shield bar
    draw->AddRectFilled(
        ImVec2(barX - 2, barY),
        ImVec2(barX, y + height),
        IM_COL32(40, 120, 255, 255)
    );
}

inline void ESPRenderer::DrawDistance(const Vector2& position, float distance) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    
    char distText[32];
    sprintf_s(distText, "%.0fm", distance / 100.0f);
    
    ImVec2 textSize = ImGui::CalcTextSize(distText);
    draw->AddText(
        ImVec2(position.x - textSize.x / 2.0f, position.y + 5),
        IM_COL32(255, 255, 255, 255),
        distText
    );
}

inline void ESPRenderer::RenderESP(const std::vector<Entity>& entities, const Vector3& localPos, const Matrix& viewMatrix) {
    for (const auto& entity : entities) {
        // World to screen
        Vector2 head = WorldToScreen(entity.position + Vector3(0, 0, 70), viewMatrix, (float)screenWidth, (float)screenHeight);
        Vector2 feet = WorldToScreen(entity.position, viewMatrix, (float)screenWidth, (float)screenHeight);
        
        // Validate screen coordinates
        if (!head.IsValid((float)screenWidth, (float)screenHeight) || 
            !feet.IsValid((float)screenWidth, (float)screenHeight)) {
            continue;
        }
        
        // Draw box
        DrawBox(head, feet, IM_COL32(255, 255, 255, 255));
        
        // Draw health bar
        if (entity.maxHealth > 0) {
            DrawHealthBar(head, feet, entity.health, entity.maxHealth);
        }
        
        // Draw shield bar
        if (entity.maxShield > 0) {
            DrawShieldBar(head, feet, entity.shield, entity.maxShield, entity.maxHealth > 0);
        }
        
        // Draw distance
        DrawDistance(feet, entity.distance);
    }
}
