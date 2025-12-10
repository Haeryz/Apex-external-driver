#pragma once
#include "../memory/offsets.h"
#include "../memory/memory_reader.h"
#include "../OS-ImGui/imgui/imgui.h"
#include <string>
#include <vector>

// Global variables from main.cpp
extern uintptr_t g_ApexBase;
extern DWORD g_ApexPid;
extern DriverInterface* g_Driver;

struct SpectatorInfo {
    std::string name;
    int index;
    
    SpectatorInfo(const std::string& n, int idx) : name(n), index(idx) {}
};

class SpectatorList {
private:
    std::vector<SpectatorInfo> spectators;
    float lastUpdateTime;
    const float UPDATE_INTERVAL = 1.0f; // Update every 1 second
    
public:
    SpectatorList() : lastUpdateTime(0.0f) {}
    
    void Update(uintptr_t localPlayerAddress) {
        // Throttle updates to reduce CPU usage
        float currentTime = ImGui::GetTime();
        if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
            return;
        }
        lastUpdateTime = currentTime;
        
        spectators.clear();
        
        // Safety checks - don't crash if something is invalid
        if (!g_Driver || !g_ApexBase || !g_ApexPid || !localPlayerAddress) {
            return;
        }
        
        // Read observer list base
        uintptr_t observerList = 0;
        
        // SAFETY: Try-catch for spectator list reading (can fail in lobby/loading)
        try {
            if (!g_Driver->ReadMemory(g_ApexPid, g_ApexBase + OFF_OBSERVER_LIST, &observerList, sizeof(observerList))) {
                return; // Silent fail - just don't show spectators
            }
            
            // Validate observer list pointer
            if (observerList == 0 || observerList < 0x10000 || observerList > 0x7FFFFFFFFFFF) {
                return; // Invalid pointer - silent fail
            }
        } catch (...) {
            // Catch any exception and return safely
            return;
        }
        
        // Check all possible spectators (max 100 players)
        for (int i = 1; i < 100; i++) {
            // Get entity address
            uintptr_t entityListAddr = g_ApexBase + OFF_ENTITY_LIST + (static_cast<uint64_t>(i) * 0x20);
            uintptr_t entityAddress = 0;
            
            if (!g_Driver->ReadMemory(g_ApexPid, entityListAddr, &entityAddress, sizeof(entityAddress))) {
                continue;
            }
            
            if (entityAddress == 0 || entityAddress < 0x10000) {
                continue;
            }
            
            // Check if this entity is dead (spectators are dead)
            int lifeState = 0;
            if (!g_Driver->ReadMemory(g_ApexPid, entityAddress + OFF_LIFE_STATE, &lifeState, sizeof(lifeState))) {
                continue;
            }
            
            // Skip alive players
            if (lifeState == 0) {
                continue;
            }
            
            // Read player data for spectator check
            int playerData = 0;
            if (!g_Driver->ReadMemory(g_ApexPid, entityAddress + 0x38, &playerData, sizeof(playerData))) {
                continue;
            }
            
            // Read spectator index
            int specIndex = 0;
            uintptr_t specIndexAddr = observerList + static_cast<uint64_t>(playerData) * 8 + OFF_OBSERVER_ARRAY;
            if (!g_Driver->ReadMemory(g_ApexPid, specIndexAddr, &specIndex, sizeof(specIndex))) {
                continue;
            }
            
            // Get the entity they are spectating
            uintptr_t spectatorTarget = 0;
            uintptr_t targetAddr = g_ApexBase + OFF_ENTITY_LIST + (static_cast<uint64_t>(specIndex & 0xFFFF) * 0x20);
            if (!g_Driver->ReadMemory(g_ApexPid, targetAddr, &spectatorTarget, sizeof(spectatorTarget))) {
                continue;
            }
            
            // Check if they are spectating the local player
            if (spectatorTarget == localPlayerAddress) {
                // Read the spectator's name
                uintptr_t nameListAddress = g_ApexBase + OFF_NAME_LIST + (static_cast<uint64_t>(i) * 24);
                uintptr_t namePtr = 0;
                
                if (g_Driver->ReadMemory(g_ApexPid, nameListAddress, &namePtr, sizeof(namePtr))) {
                    if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                        char nameBuffer[32] = {0};
                        if (g_Driver->ReadMemory(g_ApexPid, namePtr, nameBuffer, 32)) {
                            nameBuffer[31] = '\0';
                            
                            // Validate name
                            bool isValid = true;
                            int len = 0;
                            for (int j = 0; j < 32; j++) {
                                if (nameBuffer[j] == '\0') break;
                                if (nameBuffer[j] >= 0x20 && nameBuffer[j] <= 0x7E) {
                                    len++;
                                } else {
                                    isValid = false;
                                    break;
                                }
                            }
                            
                            if (isValid && len > 0 && len <= 31) {
                                nameBuffer[len] = '\0';
                                spectators.push_back(SpectatorInfo(std::string(nameBuffer), i));
                            }
                        }
                    }
                }
            }
        }
    }
    
    void Render() {
        if (spectators.empty()) {
            return;
        }
        
        // Position: Top-left corner
        ImVec2 pos(20, 20);
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        
        // Background box
        float boxWidth = 250.0f;
        float boxHeight = 30.0f + (spectators.size() * 20.0f);
        
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + boxWidth, pos.y + boxHeight),
            IM_COL32(0, 0, 0, 180),
            5.0f
        );
        
        // Border
        drawList->AddRect(
            pos,
            ImVec2(pos.x + boxWidth, pos.y + boxHeight),
            IM_COL32(255, 0, 0, 255),
            5.0f,
            0,
            2.0f
        );
        
        // Title
        ImVec2 textPos(pos.x + 10, pos.y + 8);
        char title[64];
        sprintf_s(title, "SPECTATORS [%d]", static_cast<int>(spectators.size()));
        drawList->AddText(textPos, IM_COL32(255, 0, 0, 255), title);
        
        // Spectator names
        textPos.y += 22;
        for (const auto& spec : spectators) {
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), spec.name.c_str());
            textPos.y += 20;
        }
    }
    
    int GetSpectatorCount() const {
        return static_cast<int>(spectators.size());
    }
};
