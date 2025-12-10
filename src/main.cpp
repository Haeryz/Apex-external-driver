#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <time.h>
#include "driver/driver_interface.h"
#include "memory/memory_reader.h"
#include "memory/offsets.h"
#include "game/entity.h"
#include "game/math.h"
#include "OS-ImGui/OS-ImGui.h"
#include "security/process_protection.h"
#include "game/feature_manager.h"
// Gameplay features removed for stealth (ESP-only mode)
// #include "game/tapstrafe.h"
// #include "game/aimbot.h"
#include "game/triggerbot.h"
#include "game/esp_feature.h"
#include "game/distance_manager.h"
#include <random>

// Random process name selection for stealth (common Windows processes)
struct ProcessInfo {
    const wchar_t* name;
    const wchar_t* path;
};

const ProcessInfo g_ProcessList[] = {
    { L"svchost.exe", L"C:\\Windows\\System32\\svchost.exe" },
    { L"explorer.exe", L"C:\\Windows\\explorer.exe" },
    { L"dwm.exe", L"C:\\Windows\\System32\\dwm.exe" },
    { L"RuntimeBroker.exe", L"C:\\Windows\\System32\\RuntimeBroker.exe" },
    { L"SearchIndexer.exe", L"C:\\Windows\\System32\\SearchIndexer.exe" },
    { L"taskhostw.exe", L"C:\\Windows\\System32\\taskhostw.exe" }
};

int g_SelectedProcessIndex = -1;

// Select random process at startup
void SelectRandomProcess() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(g_ProcessList) / sizeof(g_ProcessList[0]) - 1);
    g_SelectedProcessIndex = dis(gen);
}

// Forward declaration
bool IsRunningFromTemp();

// Global instances
DriverInterface* g_Driver = nullptr;
Overlay* g_Overlay = nullptr;
DWORD g_ApexPid = 0;
uintptr_t g_ApexBase = 0;

// Feature system
FeatureManager* g_FeatureManager = nullptr;
// Gameplay features removed for stealth (ESP-only mode)
// TapStrafe* g_TapStrafe = nullptr;
// Aimbot* g_Aimbot = nullptr;
TriggerBot* g_TriggerBot = nullptr;
ESPFeature* g_ESP = nullptr;
DistanceManager* g_DistanceManager = nullptr;

// Shared data
std::vector<Entity> g_Entities;
std::mutex g_EntityMutex;
Vector3 g_LocalPos;
uintptr_t g_LocalPlayerAddress = 0;
int g_LocalTeam = 0;
Matrix g_ViewMatrix;
bool g_IsRunning = true;

// Weapon data for prediction
float g_ProjectileSpeed = 30000.0f; // Default projectile speed
float g_ProjectileScale = 1.0f;     // Default gravity scale

// Screen size (will be updated by OS-ImGui)
Vector2 g_ScreenSize = { 1920, 1080 };

// Menu state
bool g_ShowMenu = false;

// FPS Limiter settings (optimized for lower CPU usage)
int g_TargetFPS = 30;  // Default: 30 FPS (was 60, reduced for performance)
int g_SleepTime = 33;   // Calculated from target FPS (was 16ms)

// Gameplay hotkeys removed (ESP-only mode)
void ProcessSettingsHotkeys() {
    // No gameplay features active
}

bool InitializeDriver() {
    std::cout << "[*] Initializing driver interface..." << std::endl;
    g_Driver = new DriverInterface();
    if (!g_Driver->Initialize()) {
        std::cout << "[-] Driver initialization failed! Make sure driver is loaded." << std::endl;
        std::cout << "[*] Waiting 5 more seconds and retrying..." << std::endl;
        Sleep(5000);
        
        // Retry once more
        delete g_Driver;
        g_Driver = new DriverInterface();
        if (!g_Driver->Initialize()) {
            MessageBoxA(nullptr, "Driver initialization failed!\n\nMake sure:\n1. Driver is loaded with kdmapper\n2. Wait 5+ seconds after loading\n3. Run as Administrator", "Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    std::cout << "[+] Driver interface initialized!" << std::endl;
    return true;
}

bool WaitForApex() {
    while (true) {
        g_ApexPid = g_Driver->GetProcessId(L"r5apex_dx12.exe");
        if (g_ApexPid) break;
        Sleep(1000);
    }
    
    g_Driver->SetCurrentPid(g_ApexPid);
    return true;
}

bool GetModuleBase() {
    // Get base address for r5apex_dx12.exe process
    g_ApexBase = g_Driver->GetModuleBase(g_ApexPid, L"r5apex_dx12.exe");
    
    if (!g_ApexBase || g_ApexBase < 0x10000) {
        return false;
    }
    
    g_Driver->GetCR3(g_ApexPid);
    return true;
}

bool InitializeFeatures() {
    g_FeatureManager = new FeatureManager();
    
    // Initialize Overlay
    g_Overlay = new Overlay();

    // ESP-ONLY MODE (Stealth - no gameplay modifications)
    // No hotkeys - only menu control
    g_ESP = new ESPFeature();
    g_FeatureManager->RegisterFeature("Visuals", g_ESP, 0);
    
    // Initialize Distance Manager
    g_DistanceManager = new DistanceManager();
    g_FeatureManager->RegisterFeature("Distance", g_DistanceManager, 0);
    
    // Initialize TriggerBot (kernel-level mouse injection)
    g_TriggerBot = new TriggerBot(g_Driver, g_Overlay);
    g_FeatureManager->RegisterFeature("TriggerBot", g_TriggerBot, 0);
    
    return true;
}

Vector3 GetLocalPlayerPosition() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return Vector3();
    return Read<Vector3>(localPlayer + OFF_LOCAL_ORIGIN);
}

int GetLocalPlayerTeam() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return 0;
    return Read<int>(localPlayer + OFF_TEAM_NUMBER);
}

void UpdateWeaponInfo() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return;
    
    // Get weapon handle
    uintptr_t weaponHandle = Read<uintptr_t>(localPlayer + OFF_WEAPON_HANDLE);
    if (!weaponHandle || weaponHandle == 0xFFFFFFFF) {
        g_ProjectileSpeed = 30000.0f; // Default
        g_ProjectileScale = 1.0f;
        return;
    }
    
    // Calculate weapon entity address
    uintptr_t weaponEntity = Read<uintptr_t>(g_ApexBase + OFF_ENTITY_LIST + ((weaponHandle & 0xFFFF) << 5));
    if (!weaponEntity) {
        g_ProjectileSpeed = 30000.0f;
        g_ProjectileScale = 1.0f;
        return;
    }
    
    // Read projectile speed and scale
    g_ProjectileSpeed = Read<float>(weaponEntity + OFF_PROJECTILESPEED);
    g_ProjectileScale = Read<float>(weaponEntity + OFF_PROJECTILESCALE);
    
    // Sanity check
    if (g_ProjectileSpeed <= 0 || g_ProjectileSpeed > 100000.0f) {
        g_ProjectileSpeed = 30000.0f;
    }
    if (g_ProjectileScale <= 0 || g_ProjectileScale > 10.0f) {
        g_ProjectileScale = 1.0f;
    }
}

Matrix GetViewMatrix() {
    uintptr_t viewRender = Read<uintptr_t>(g_ApexBase + OFF_VIEWRENDER);
    if (!viewRender) return Matrix();
    
    uintptr_t viewMatrixPtr = Read<uintptr_t>(viewRender + OFF_VIEWMATRIX);
    if (!viewMatrixPtr) return Matrix();
    
    Matrix matrix;
    ReadArray<float>(viewMatrixPtr, matrix.matrix, 16);
    return matrix;
}

// Thread for reading memory and updating features
void MemoryLoop() {
    while (g_IsRunning) {
        if (g_ApexBase != 0) {
            uintptr_t localPlayer = 0;
            
            try {
                // Read local player pointer (needed for features)
                localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
                
                // Validate local player pointer before continuing
                if (!localPlayer || localPlayer < 0x10000) {
                    Sleep(g_SleepTime);
                    continue;
                }
                
                // Read local data for ESP
                Vector3 localPos = GetLocalPlayerPosition();
                int localTeam = GetLocalPlayerTeam();
                Matrix viewMatrix = GetViewMatrix();
                
                // Update weapon info for prediction
                UpdateWeaponInfo();
                
                // Scan entities (64 max players)
                auto entities = EntityScanner::ScanEntities(64);
                
                // Update shared data
                {
                    std::lock_guard<std::mutex> lock(g_EntityMutex);
                    g_LocalPos = localPos;
                    g_LocalPlayerAddress = localPlayer;
                    g_LocalTeam = localTeam;
                    g_ViewMatrix = viewMatrix;
                    g_Entities = entities;
                }
                
                // Update features (requires local player)
                if (g_FeatureManager && localPlayer) {
                    g_FeatureManager->UpdateAllFeatures(localPlayer, g_ApexBase);
                }
            } catch (...) {
                // Catch any exception and continue - don't crash
                // This can happen during map transitions
            }

            // Hotkeys handled via WM_HOTKEY messages now
            // No polling needed (RegisterHotKey)
        }
        Sleep(g_SleepTime); // Dynamic FPS limiter
    }
}

// Menu rendering
void RenderMenu() {
    if (!g_ShowMenu) return;
    
    // Static tab index for tracking active page
    static int activeTab = 0;
    
    ImGuiStyle* style = &ImGui::GetStyle();
    
    // Save original style
    ImVec4 originalSeparator = style->Colors[ImGuiCol_Separator];
    ImVec4 originalBorder = style->Colors[ImGuiCol_Border];
    ImVec2 originalItemSpacing = style->ItemSpacing;
    ImVec2 originalFramePadding = style->FramePadding;
    float originalFrameBorderSize = style->FrameBorderSize;
    
    // Apply overlayexp styling (NO borders, NO separators)
    style->WindowPadding = ImVec2(6, 6);
    style->Colors[ImGuiCol_Separator] = ImVec4(0, 0, 0, 0); // Transparent separator
    style->Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);    // No borders
    style->FrameBorderSize = 0.0f;                           // No button borders
    
    // Overlayexp style colors
    ImVec4 bg_color = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.00f);
    ImVec4 button_color = ImVec4(12.f/255.f, 12.f/255.f, 12.f/255.f, 1.0f);
    ImVec4 button_hover = ImVec4(22.f/255.f, 22.f/255.f, 22.f/255.f, 1.0f);
    ImVec4 button_active = ImVec4(20.f/255.f, 20.f/255.f, 20.f/255.f, 1.0f);
    
    ImGui::SetNextWindowSize(ImVec2(660.f, 560.f));
    if (ImGui::Begin("VGKASS", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar)) {
        
        ImGui::BeginChild("Complete Border", ImVec2(648.f, 548.f), false);
        {
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 image_size = ImVec2(648.f, 548.f);
            ImVec2 image_pos;
            image_pos.x = window_pos.x + (window_size.x - image_size.x) * 0.5f;
            image_pos.y = window_pos.y + (window_size.y - image_size.y) * 0.5f;
            
            // Same background as overlayexp
            ImGui::GetWindowDrawList()->AddRectFilled(image_pos, 
                ImVec2(image_pos.x + image_size.x, image_pos.y + image_size.y), 
                IM_COL32(10, 10, 10, 255));
        }
        ImGui::EndChild();
        
        ImGui::SameLine(6.f);
        
        style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
        style->ItemSpacing = ImVec2(0.f, 0.f);
        
        ImGui::BeginChild("Menu", ImVec2(648.f, 548.f), false);
        {
            ImGui::Columns(2, nullptr, false); // FALSE = no column separator line
            ImGui::SetColumnWidth(0, 78.f);
            style->ItemSpacing = ImVec2(0.f, -1.f);
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImVec2(75, 548.f);
            
            // Sidebar same color as main background (NO borders)
            ImU32 color = IM_COL32(12, 12, 12, 255);
            ImVec2 shifted_pos = ImVec2(pos.x, pos.y + 1);
            draw_list->AddRectFilled(shifted_pos, ImVec2(shifted_pos.x + size.x, shifted_pos.y + size.y), color);
            
            ImVec2 cursor_pos = ImGui::GetCursorPos();
            
            // Sidebar buttons - simple text (like overlayexp) NO BORDERS
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f); // NO borders
            ImGui::PushStyleColor(ImGuiCol_Button, button_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0)); // Transparent border
            
            if (ImGui::GetIO().Fonts->Fonts.Size > 3) {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
            }
            
            // Button A (Visuals)
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 2;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("V", ImVec2(75.f, 75.f))) activeTab = 0;
            }
            
            // Button D (Misc)
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 79;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("M", ImVec2(75.f, 75.f))) activeTab = 1;
            }
            
            // Button T (TriggerBot)
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 156;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("T", ImVec2(75.f, 75.f))) activeTab = 2;
            }
            
            // Button B (Settings)
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 233;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("S", ImVec2(75.f, 75.f))) activeTab = 3;
            }
            
            if (ImGui::GetIO().Fonts->Fonts.Size > 3) {
                ImGui::PopFont();
            }
            ImGui::PopStyleColor(4); // Pop 4 colors now
            ImGui::PopStyleVar(2);    // Pop 2 style vars
            
            ImGui::NextColumn();
            
            // Main content area
            ImGui::BeginChild("MainContent");
            {
                style->ItemSpacing = ImVec2(8, 8); // Better spacing
                style->FramePadding = ImVec2(8, 4);
                
                if (activeTab == 0) // Visuals
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("V I S U A L S");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // ESP Enable/Disable
                    if (g_ESP) {
                        bool espEnabled = g_ESP->IsEnabled();
                        ImGui::Text("ESP Status");
                        if (ImGui::Checkbox("##espenable", &espEnabled)) {
                            g_ESP->SetEnabled(espEnabled);
                        }
                        ImGui::SameLine();
                        ImGui::Text(espEnabled ? "Enabled" : "Disabled");
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Distance slider (100-1000m)
                    ImGui::Text("Max Distance");
                    static int maxDistance = 500;
                    if (g_DistanceManager) {
                        maxDistance = (int)g_DistanceManager->GetMaxDistance();
                    }
                    if (ImGui::SliderInt("##maxdist", &maxDistance, 100, 1000, "%dm")) {
                        if (g_DistanceManager) {
                            g_DistanceManager->SetMaxDistance((float)maxDistance);
                        }
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // ESP Feature Toggles
                    ImGui::Text("ESP Features");
                    ImGui::Spacing();
                    
                    if (g_ESP) {
                        ImGui::Checkbox("Box ESP", &g_ESP->config.showBoxes);
                        ImGui::Checkbox("Health Bars", &g_ESP->config.showHealthBars);
                        ImGui::Checkbox("Shield Bars", &g_ESP->config.showShieldBars);
                        ImGui::Checkbox("Distance", &g_ESP->config.showDistances);
                        ImGui::Checkbox("Knocked State", &g_ESP->config.showKnocked);
                    }
                }
                else if (activeTab == 1) // Misc
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("M I S C");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Status");
                    ImGui::Spacing();
                    ImGui::BulletText("Driver Connected");
                    ImGui::BulletText("Game: %s", g_ApexBase ? "Apex Legends" : "Waiting...");
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Info");
                    ImGui::Spacing();
                    ImGui::BulletText("VGKASS EXTERNAL");
                    ImGui::BulletText("StreamProof");
                    ImGui::BulletText("Undetected");
                }
                else if (activeTab == 2) // TriggerBot
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("T R I G G E R B O T");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    if (g_TriggerBot) {
                        g_TriggerBot->RenderMenu();
                    }
                }
                else if (activeTab == 3) // Settings
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Text("S E T T I N G S");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Performance");
                    ImGui::SliderInt("Memory Loop FPS", &g_TargetFPS, 30, 240);
                    
                    // Update sleep time based on target FPS
                    g_SleepTime = (g_TargetFPS > 0) ? (1000 / g_TargetFPS) : 33;
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Overlay Info");
                    ImGui::Spacing();
                    ImGui::BulletText("FPS: %.1f", ImGui::GetIO().Framerate);
                    ImGui::BulletText("Hotkey: HOME");
                }
            }
            ImGui::EndChild();
            
            ImGui::Columns(1);
        }
        ImGui::EndChild();
    }
    ImGui::End();
    
    // Restore original style
    style->Colors[ImGuiCol_Separator] = originalSeparator;
    style->Colors[ImGuiCol_Border] = originalBorder;
    style->ItemSpacing = originalItemSpacing;
    style->FramePadding = originalFramePadding;
    style->FrameBorderSize = originalFrameBorderSize;
}

// Render function called by OS-ImGui
void RenderESP() {
    // Wrap entire render in try-catch to prevent overlay crashes
    try {
        // Update screen size
        g_ScreenSize.x = ImGui::GetIO().DisplaySize.x;
        g_ScreenSize.y = ImGui::GetIO().DisplaySize.y;
        
        // Check for L key to toggle menu (using Windows API for reliable detection on transparent overlay)
        static bool toggleKeyWasPressed = false;
        bool toggleKeyIsPressed = (GetAsyncKeyState('L') & 0x8000) != 0;
        
        if (toggleKeyIsPressed && !toggleKeyWasPressed) {
            g_ShowMenu = !g_ShowMenu;
        }
        toggleKeyWasPressed = toggleKeyIsPressed;
        
        // Render menu if open (ONLY visible when " is pressed)
        RenderMenu();
        
        if (g_ApexBase == 0) return;
        
        // Copy data to avoid locking for too long
        std::vector<Entity> entities;
        Vector3 localPos;
        int localTeam;
        Matrix viewMatrix;
        
        {
            std::lock_guard<std::mutex> lock(g_EntityMutex);
            entities = g_Entities;
            localPos = g_LocalPos;
            localTeam = g_LocalTeam;
            viewMatrix = g_ViewMatrix;
        }
        
        // Validate we have valid data before rendering
        if (localPos.x == 0 && localPos.y == 0 && localPos.z == 0) {
            return; // Invalid local position - don't render
        }
    
        // Render ESP (Unicode obfuscation provides OCR protection)
        if (g_ESP && g_ESP->IsEnabled()) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            
            // Get current max distance from distance manager
            float maxDistanceMeters = g_DistanceManager ? g_DistanceManager->GetMaxDistance() : 500.0f;
            float maxDistanceInches = maxDistanceMeters * 39.37f; // Convert to game units
            
            for (auto& entity : entities) {
                if (!entity.IsValidForESP(localPos, localTeam, maxDistanceInches)) continue;
                
                // World to screen
                Vector2 head = WorldToScreen(entity.position + Vector3(0, 0, 70), viewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                Vector2 feet = WorldToScreen(entity.position, viewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                
                if (!head.IsValid(g_ScreenSize.x, g_ScreenSize.y) || !feet.IsValid(g_ScreenSize.x, g_ScreenSize.y)) continue;
                
                float height = feet.y - head.y;
                float width = height / 2.0f;
                float x = feet.x - width / 2.0f;
                float y = head.y;
                
                ImVec2 topLeft(x, y);
                ImVec2 bottomRight(x + width, y + height);
                
                // Determine color (enemy/knocked/visible) - Only enemies shown
                ImU32 boxColor;
                if (entity.isKnocked) {
                    boxColor = IM_COL32(128, 128, 128, 255); // Gray for knocked
                } else if (entity.isVisible) {
                    boxColor = IM_COL32(255, 255, 0, 255); // Yellow for visible enemies
                } else {
                    boxColor = IM_COL32(255, 0, 0, 255); // Red for enemies
                }
                
                // Draw Box (with better styling)
                if (g_ESP->config.showBoxes) {
                    // Outer black border for contrast
                    drawList->AddRect(topLeft, bottomRight, IM_COL32(0, 0, 0, 255), 0, 0, g_ESP->config.boxThickness + 1.0f);
                    // Main box
                    drawList->AddRect(topLeft, bottomRight, boxColor, 0, 0, g_ESP->config.boxThickness);
                }
                
                // Draw Health & Shield Bars (DMA-style)
                if (g_ESP->config.showHealthBars || g_ESP->config.showShieldBars) {
                    g_ESP->DrawHealthAndShieldBar(drawList, topLeft, bottomRight, 
                        entity.health, entity.maxHealth, entity.shield, entity.maxShield, entity.armorType);
                }
                
                // Draw player info (distance only)
                float distanceMeters = entity.distance / 39.37f; // Convert to meters
                g_ESP->DrawPlayerInfo(drawList, ImVec2(x, y - 16), entity, distanceMeters, false);
            }
        }
    
        // Render TriggerBot hitboxes
        if (g_TriggerBot && g_TriggerBot->IsEnabled()) {
            g_TriggerBot->RenderHitboxes();
        }
    
    } catch (...) {
        // Catch any rendering exception - don't crash overlay
        // This can happen during transitions or invalid data
    }
}

// Check if already running from temp (to avoid infinite loop)
bool IsRunningFromTemp() {
    WCHAR currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);
    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    return wcsstr(currentPath, tempPath) != nullptr;
}

// Copy self to temp with random name and execute
bool CopyToTempAndExecute() {
    // Get current executable path
    WCHAR currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);
    
    // Get temp directory
    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    // Generate random filename from process list
    SelectRandomProcess();
    if (g_SelectedProcessIndex < 0) return false;
    
    const ProcessInfo& selectedProcess = g_ProcessList[g_SelectedProcessIndex];
    
    // Create temp file path
    WCHAR tempExePath[MAX_PATH];
    swprintf_s(tempExePath, MAX_PATH, L"%s%s", tempPath, selectedProcess.name);
    
    // Copy file to temp
    if (!CopyFileW(currentPath, tempExePath, FALSE)) {
        return false;
    }
    
    // Execute the temp copy
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (!CreateProcessW(tempExePath, NULL, NULL, NULL, FALSE, 
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DeleteFileW(tempExePath); // Clean up on failure
        return false;
    }
    
    // Close handles (we don't need them)
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ===== DISABLED: SELF-COPY TO TEMP WITH RANDOM NAME =====
    // This was causing issues - disabled for debugging
    /*
    if (!IsRunningFromTemp()) {
        // We're running from original location - copy to temp and execute
        if (CopyToTempAndExecute()) {
            // Successfully launched temp copy, exit this instance
            return 0;
        }
        // If copy failed, continue with original (fallback)
    }
    */
    // If already in temp, continue normal execution
    
    // ===== DISABLED: PROCESS NAME SPOOFING =====
    // This was causing issues - disabled for debugging
    /*
    // Select random process name for spoofing IMMEDIATELY
    SelectRandomProcess();
    
    // Spoof process name in PEB BEFORE console creation
    PPEB peb = (PPEB)__readgsqword(0x60);
    if (peb && peb->ProcessParameters && g_SelectedProcessIndex >= 0) {
        const ProcessInfo& selectedProcess = g_ProcessList[g_SelectedProcessIndex];
        
        // Static buffer for spoofed path
        static WCHAR spoofedPath[MAX_PATH];
        wcscpy_s(spoofedPath, MAX_PATH, selectedProcess.path);
        
            // Spoof ImagePathName and CommandLine in PEB
        peb->ProcessParameters->ImagePathName.Buffer = spoofedPath;
        peb->ProcessParameters->ImagePathName.Length = (USHORT)(wcslen(spoofedPath) * sizeof(WCHAR));
        peb->ProcessParameters->ImagePathName.MaximumLength = peb->ProcessParameters->ImagePathName.Length + sizeof(WCHAR);
        peb->ProcessParameters->CommandLine.Buffer = spoofedPath;
        peb->ProcessParameters->CommandLine.Length = peb->ProcessParameters->ImagePathName.Length;
        peb->ProcessParameters->CommandLine.MaximumLength = peb->ProcessParameters->ImagePathName.MaximumLength;
    }
    
    // Small delay to ensure spoofing takes effect
    Sleep(100);
    */
    
    // ===== CONSOLE INITIALIZATION =====
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    
    std::cout << "========================================" << std::endl;
    std::cout << "   Apex ESP - Press L to toggle menu   " << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Debugger check - DISABLED for testing
    // if (ProcessProtection::IsBeingDebugged()) {
    //     return 1;
    // }
    
    // Initialize driver
    if (!InitializeDriver()) {
        std::cout << "[!] Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    // Wait for Apex
    std::cout << "Waiting for Apex Legends (r5apex_dx12.exe)..." << std::endl;
    if (!WaitForApex()) {
        return 1;
    }
    std::cout << "Game detected! PID: " << g_ApexPid << std::endl;
    
    // Hide console as soon as game is found
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) {
        ShowWindow(consoleWindow, SW_HIDE);
    }
    
    // Get module base with retries
    std::cout << "Getting module base..." << std::endl;
    bool baseFound = false;
    for (int i = 0; i < 10; i++) {
        if (GetModuleBase()) {
            baseFound = true;
            break;
        }
        Sleep(2000);
    }

    if (!baseFound) {
        std::cout << "Failed to get module base." << std::endl;
        Sleep(3000);
        return 1;
    }
    
    // Initialize feature system
    if (!InitializeFeatures()) {
        std::cout << "Failed to initialize features." << std::endl;
        Sleep(3000);
        return 1;
    }
    
    // Start memory thread
    std::thread memoryThread(MemoryLoop);
    memoryThread.detach();
    
    // Wait for game window
    std::cout << "Waiting for game window..." << std::endl;
    HWND apexWindow = NULL;
    while (apexWindow == NULL) {
        apexWindow = FindWindowA("Respawn001", NULL);
        if (apexWindow == NULL) {
            Sleep(2000);
        }
    }
    
    std::cout << "Window found! Starting overlay..." << std::endl;
    Sleep(3000);
    
    // Attach overlay
    bool overlayStarted = false;
    for (int i = 0; i < 5; i++) {
        try {
            Gui.AttachAnotherWindow("Apex Legends", "Respawn001", RenderESP);
            overlayStarted = true;
            break;
        }
        catch (RenderCore::OSException& e) {
            if (i < 4) {
                Sleep(1000);
            }
        }
        catch (...) {
            break;
        }
    }
    
    if (!overlayStarted) {
        g_IsRunning = false;
    }
    
    g_IsRunning = false;
    
    // Cleanup
    if (g_ESP) {
        delete g_ESP;
        g_ESP = nullptr;
    }
    
    if (g_DistanceManager) {
        delete g_DistanceManager;
        g_DistanceManager = nullptr;
    }
    
    if (g_FeatureManager) {
        delete g_FeatureManager;
        g_FeatureManager = nullptr;
    }
    
    if (g_Driver) {
        g_Driver->Cleanup();
        delete g_Driver;
    }
    
    return 0;
}
