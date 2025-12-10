#include "triggerbot.h"
#include "math.h"
#include <cmath>
#include <algorithm>
#include "../OS-ImGui/imgui/imgui.h"
#include <mutex>

TriggerBot::TriggerBot(DriverInterface* drv, Overlay* ovl) 
    : driver(drv), overlay(ovl), enabled(false), lastTriggerTime(0), 
      gen(rd()), delayLevel(TriggerDelayLevel::LEVEL_3), boneMode(TriggerBoneMode::HEAD_BODY),
      showHitboxes(true), usePrediction(true), extraPadding(0.1f), closeRangeThreshold(4.0f),
      triggerKey(VK_XBUTTON2), isSelectingKey(false) {
}

TriggerBot::~TriggerBot() {
}

int TriggerBot::GetRandomDelay() {
    // Get delay range based on level
    int minDelay, maxDelay;
    
    switch (delayLevel) {
        case TriggerDelayLevel::LEVEL_1:
            minDelay = 200; maxDelay = 300;
            break;
        case TriggerDelayLevel::LEVEL_2:
            minDelay = 100; maxDelay = 200;
            break;
        case TriggerDelayLevel::LEVEL_3:
            minDelay = 50; maxDelay = 100;
            break;
        case TriggerDelayLevel::LEVEL_4:
            minDelay = 25; maxDelay = 50;
            break;
        case TriggerDelayLevel::LEVEL_5:
            minDelay = 10; maxDelay = 25;
            break;
        default:
            minDelay = 50; maxDelay = 100;
            break;
    }
    
    std::uniform_int_distribution<> dist(minDelay, maxDelay);
    return dist(gen);
}

std::vector<int> TriggerBot::GetActiveBones() {
    switch (boneMode) {
        case TriggerBoneMode::BODY_ONLY:
            return { 2, 3 }; // Chest, Pelvis
        case TriggerBoneMode::HEAD_ONLY:
            return { 0 }; // Head only
        case TriggerBoneMode::HEAD_BODY:
            return { 0, 2 }; // Head, Chest (priority order)
        case TriggerBoneMode::FULL_BODY:
            return { 0, 1, 2, 3 }; // Head, Neck, Chest, Pelvis
        default:
            return { 0, 2 }; // Default: Head + Body
    }
}

TriggerVec3 TriggerBot::GetBoxDimensionsForBone(int boneIndex) {
    switch (boneIndex) {
        case 0: // Head
            return { 5.0f, 5.0f, 5.0f };
        case 1: // Neck
            return { 4.0f, 4.0f, 4.0f };
        case 2: // Chest
            return { 7.0f, 7.0f, 10.0f };
        case 3: // Pelvis
            return { 8.0f, 8.0f, 12.0f };
        default:
            return { 6.0f, 6.0f, 8.0f };
    }
}

std::vector<TriggerVec3> TriggerBot::CalculateBoxCorners(const TriggerVec3& bonePos, const TriggerVec3& dimensions) {
    return {
        // Front 4 corners
        {bonePos.x + dimensions.x, bonePos.y + dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y + dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y - dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x + dimensions.x, bonePos.y - dimensions.y, bonePos.z + dimensions.z},
        // Back 4 corners
        {bonePos.x + dimensions.x, bonePos.y + dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y + dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y - dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x + dimensions.x, bonePos.y - dimensions.y, bonePos.z - dimensions.z}
    };
}

TriggerVec3 TriggerBot::CalculatePredictedPosition(const TriggerVec3& bonePos, const TriggerVec3& velocity, float projectileSpeed) {
    if (projectileSpeed <= 0.0f) return bonePos;
    
    // Get local player position
    extern Vector3 g_LocalPos;
    TriggerVec3 localPos = { g_LocalPos.x, g_LocalPos.y, g_LocalPos.z };
    
    // Calculate distance to target
    float distance = sqrt(pow(bonePos.x - localPos.x, 2) + 
                         pow(bonePos.y - localPos.y, 2) + 
                         pow(bonePos.z - localPos.z, 2));
    
    // Calculate time for bullet to reach target
    float timeToHit = distance / projectileSpeed;
    
    // Predict future position based on velocity
    return {
        bonePos.x + velocity.x * timeToHit,
        bonePos.y + velocity.y * timeToHit,
        bonePos.z + velocity.z * timeToHit
    };
}

bool TriggerBot::CheckBoneOnCrosshair(Entity& entity, int boneIndex) {
    if (!overlay) return false;
    
    // Get bone position from Entity
    Vector3 bonePos = entity.GetBonePosition(boneIndex);
    if (bonePos.x == 0 && bonePos.y == 0 && bonePos.z == 0) return false;
    
    // Convert to TriggerVec3 for calculations
    TriggerVec3 bonePosVec3 = { bonePos.x, bonePos.y, bonePos.z };
    
    // Apply prediction if enabled
    if (usePrediction) {
        extern float g_ProjectileSpeed;
        TriggerVec3 velocityVec3 = { entity.velocity.x, entity.velocity.y, entity.velocity.z };
        bonePosVec3 = CalculatePredictedPosition(bonePosVec3, velocityVec3, g_ProjectileSpeed);
    }
    
    // Get box dimensions
    TriggerVec3 boxDimensions = GetBoxDimensionsForBone(boneIndex);
    
    // Apply extra padding
    boxDimensions.x *= (1.0f + extraPadding);
    boxDimensions.y *= (1.0f + extraPadding);
    boxDimensions.z *= (1.0f + extraPadding);
    
    // Calculate 3D bounding box corners
    std::vector<TriggerVec3> corners = CalculateBoxCorners(bonePosVec3, boxDimensions);
    
    // Get view matrix for world to screen
    extern Matrix g_ViewMatrix;
    extern Vector2 g_ScreenSize;
    
    // Project to screen and find min/max bounds
    float minX = FLT_MAX, maxX = -FLT_MAX, minY = FLT_MAX, maxY = -FLT_MAX;
    bool anyVisible = false;
    
    for (const auto& corner : corners) {
        // Convert TriggerVec3 to Vector3 for WorldToScreen
        Vector3 cornerV3 = { corner.x, corner.y, corner.z };
        Vector2 screenPos = WorldToScreen(cornerV3, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
        
        if (screenPos.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
            anyVisible = true;
            minX = (std::min)(minX, screenPos.x);
            maxX = (std::max)(maxX, screenPos.x);
            minY = (std::min)(minY, screenPos.y);
            maxY = (std::max)(maxY, screenPos.y);
        }
    }
    
    if (!anyVisible) return false;
    
    // Close range expansion
    float distance = entity.distance / 39.37f; // Convert to meters
    if (distance <= closeRangeThreshold) {
        float expansionFactor = 1.0f + (closeRangeThreshold - distance) / closeRangeThreshold;
        float width = maxX - minX;
        float height = maxY - minY;
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        
        minX = centerX - width * 0.5f * expansionFactor;
        maxX = centerX + width * 0.5f * expansionFactor;
        minY = centerY - height * 0.5f * expansionFactor;
        maxY = centerY + height * 0.5f * expansionFactor;
    }
    
    // Get screen center (crosshair)
    float screenCenterX = g_ScreenSize.x / 2.0f;
    float screenCenterY = g_ScreenSize.y / 2.0f;
    
    // Check if crosshair is within bounds
    return (screenCenterX >= minX && screenCenterX <= maxX &&
            screenCenterY >= minY && screenCenterY <= maxY);
}

bool TriggerBot::ShouldTrigger() {
    // Get entities from global shared memory
    extern std::vector<Entity> g_Entities;
    extern std::mutex g_EntityMutex;
    extern Vector3 g_LocalPos;
    extern int g_LocalTeam;
    
    std::vector<Entity> entities;
    {
        std::lock_guard<std::mutex> lock(g_EntityMutex);
        entities = g_Entities;
    }
    
    std::vector<int> bones = GetActiveBones();
    
    for (auto& entity : entities) {
        // Filter: only enemies, not knocked, alive
        if (entity.team == g_LocalTeam) continue;
        if (entity.isKnocked) continue;
        if (entity.health <= 0) continue;
        
        // Check each active bone
        for (int boneIndex : bones) {
            if (CheckBoneOnCrosshair(entity, boneIndex)) {
                return true;
            }
        }
    }
    
    return false;
}

void TriggerBot::Update(uintptr_t localPlayerPtr, uintptr_t gameBase) {
    if (!enabled || !driver) return;
    
    // Check if key is held (if key is set)
    if (triggerKey != 0 && !(GetAsyncKeyState(triggerKey) & 0x8000)) {
        return;
    }
    
    DWORD currentTime = GetTickCount64();
    
    // Get random delay based on level
    int randomDelay = GetRandomDelay();
    if (currentTime - lastTriggerTime < randomDelay) return;
    
    // Check if should trigger
    if (ShouldTrigger()) {
        // Inject mouse click via kernel driver
        if (driver->InjectMouseClick()) {
            lastTriggerTime = currentTime;
        }
    }
}

void TriggerBot::RenderMenu() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::Checkbox("Enable Triggerbot", &enabled);
    ImGui::PopStyleVar();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Hotkey Selection
    ImGui::Text("Trigger Key:");
    ImGui::SameLine();
    
    char keyName[32];
    if (isSelectingKey) {
        strcpy_s(keyName, "Press any key...");
    } else {
        if (triggerKey == 0) {
            strcpy_s(keyName, "None");
        } else {
            // Simple key name mapping
            switch (triggerKey) {
                case VK_LBUTTON: strcpy_s(keyName, "Left Mouse"); break;
                case VK_RBUTTON: strcpy_s(keyName, "Right Mouse"); break;
                case VK_MBUTTON: strcpy_s(keyName, "Middle Mouse"); break;
                case VK_XBUTTON1: strcpy_s(keyName, "Mouse 4"); break;
                case VK_XBUTTON2: strcpy_s(keyName, "Mouse 5"); break;
                case VK_SHIFT: strcpy_s(keyName, "Shift"); break;
                case VK_CONTROL: strcpy_s(keyName, "Control"); break;
                case VK_MENU: strcpy_s(keyName, "Alt"); break;
                default: sprintf_s(keyName, "Key %d", triggerKey); break;
            }
        }
    }
    
    if (ImGui::Button(keyName, ImVec2(120, 20))) {
        isSelectingKey = true;
    }
    
    if (isSelectingKey) {
        // Check for key presses
        for (int i = 3; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                // Ignore mouse clicks if clicking the button itself
                if (i == VK_LBUTTON) continue; 
                
                triggerKey = i;
                isSelectingKey = false;
                break;
            }
        }
        // Allow cancelling with Escape
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            isSelectingKey = false;
        }
    }
    
    ImGui::Spacing();
    
    // Delay Level Selection
    ImGui::Text("Delay Level:");
    const char* delayLevels[] = {
        "Level 1 (200-300ms - Very Safe)",
        "Level 2 (100-200ms - Safe)",
        "Level 3 (50-100ms - Balanced)",
        "Level 4 (25-50ms - Aggressive)",
        "Level 5 (10-25ms - Very Aggressive)"
    };
    int currentLevel = static_cast<int>(delayLevel);
    if (ImGui::Combo("##DelayLevel", &currentLevel, delayLevels, 5)) {
        delayLevel = static_cast<TriggerDelayLevel>(currentLevel);
    }
    
    ImGui::Spacing();
    
    // Bone Mode Selection
    ImGui::Text("Target Bones:");
    const char* boneModes[] = {
        "Body Only (Chest + Pelvis)",
        "Head Only",
        "Head + Body (Priority Head)",
        "Full Body (All Bones)"
    };
    int currentMode = static_cast<int>(boneMode);
    if (ImGui::Combo("##BoneMode", &currentMode, boneModes, 4)) {
        boneMode = static_cast<TriggerBoneMode>(currentMode);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Visual Settings
    ImGui::Text("Visual Settings:");
    ImGui::Checkbox("Show Hitboxes", &showHitboxes);
    ImGui::Checkbox("Use Prediction", &usePrediction);
    
    ImGui::Spacing();
    
    // Advanced Settings
    ImGui::Text("Advanced:");
    ImGui::SliderFloat("Extra Padding", &extraPadding, 0.0f, 0.5f, "%.2f");
    ImGui::SliderFloat("Close Range Threshold", &closeRangeThreshold, 0.0f, 10.0f, "%.1f");
}

void TriggerBot::RenderHitboxes() {
    if (!showHitboxes || !enabled) return;
    
    // Get entities from global shared memory
    extern std::vector<Entity> g_Entities;
    extern std::mutex g_EntityMutex;
    extern int g_LocalTeam;
    extern Matrix g_ViewMatrix;
    extern Vector2 g_ScreenSize;
    
    std::vector<Entity> entities;
    {
        std::lock_guard<std::mutex> lock(g_EntityMutex);
        entities = g_Entities;
    }
    
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    std::vector<int> bones = GetActiveBones();
    
    for (auto& entity : entities) {
        // Filter: only enemies
        if (entity.team == g_LocalTeam) continue;
        if (!entity.valid) continue;
        
        for (int boneIndex : bones) {
            Vector3 bonePos = entity.GetBonePosition(boneIndex);
            if (bonePos.x == 0 && bonePos.y == 0 && bonePos.z == 0) continue;
            
            TriggerVec3 bonePosVec3 = { bonePos.x, bonePos.y, bonePos.z };
            TriggerVec3 boxDimensions = GetBoxDimensionsForBone(boneIndex);
            boxDimensions.x *= (1.0f + extraPadding);
            boxDimensions.y *= (1.0f + extraPadding);
            boxDimensions.z *= (1.0f + extraPadding);
            
            std::vector<TriggerVec3> corners = CalculateBoxCorners(bonePosVec3, boxDimensions);
            
            // Draw box edges
            ImU32 color = CheckBoneOnCrosshair(entity, boneIndex) ? 
                         IM_COL32(255, 0, 0, 180) : IM_COL32(0, 255, 255, 180);
            
            for (size_t i = 0; i < 4; i++) {
                // Convert TriggerVec3 to Vector3 for WorldToScreen
                Vector3 c1 = { corners[i].x, corners[i].y, corners[i].z };
                Vector3 c2 = { corners[(i + 1) % 4].x, corners[(i + 1) % 4].y, corners[(i + 1) % 4].z };
                Vector3 c3 = { corners[i + 4].x, corners[i + 4].y, corners[i + 4].z };
                Vector3 c4 = { corners[((i + 1) % 4) + 4].x, corners[((i + 1) % 4) + 4].y, corners[((i + 1) % 4) + 4].z };
                
                Vector2 p1, p2;
                // Front face
                p1 = WorldToScreen(c1, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c2, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
                // Back face
                p1 = WorldToScreen(c3, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c4, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
                // Connecting edges
                Vector3 c5 = { corners[i].x, corners[i].y, corners[i].z };
                Vector3 c6 = { corners[i + 4].x, corners[i + 4].y, corners[i + 4].z };
                p1 = WorldToScreen(c5, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c6, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
            }
        }
    }
}
