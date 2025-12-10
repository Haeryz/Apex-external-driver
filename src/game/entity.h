#pragma once
#include "math.h"
#include "../memory/offsets.h"
#include "../memory/memory_reader.h"
#include <string>
#include <vector>

// Global variables from main.cpp
extern uintptr_t g_ApexBase;
extern DWORD g_ApexPid;
extern DriverInterface* g_Driver;

struct Entity {
    uintptr_t address;
    int index; // Entity Index
    Vector3 position;
    Vector3 velocity; // Absolute velocity (for prediction)
    int health;
    int maxHealth;
    int shield;
    int maxShield;
    int armorType; // 0=None, 1=White, 2=Blue, 3=Purple, 4=Gold/Red
    int team;
    int bleedoutState;
    bool valid;
    bool isVisible; // Line of sight check
    bool isKnocked;
    float distance;
    
    Entity() : address(0), index(0), position(), velocity(), health(0), maxHealth(0), shield(0), maxShield(0), 
               armorType(0), team(0), bleedoutState(0), valid(false), isVisible(false),
               isKnocked(false), distance(0.0f) {}
    
    void Update() {
        if (!address) {
            valid = false;
            return;
        }

        // Optimization: Read all data in one go (up to ArmorType)
        // Max offset needed is OFF_ARMORTYPE (0x4974)
        // We read 0x5000 bytes (20KB) to cover all needed offsets with safety margin
        // This reduces 10+ calls to 1 call per entity
        constexpr size_t READ_SIZE = 0x5000;
        uint8_t buffer[READ_SIZE];
        
        if (!g_Driver->ReadMemory(g_Driver->GetCurrentPid(), address, buffer, READ_SIZE)) {
            valid = false;
            return;
        }
        
        // Helper lambda to read from buffer
        auto Get = [&](uintptr_t offset) -> auto* {
            return reinterpret_cast<uint8_t*>(buffer + offset);
        };

        // Read position
        position = *reinterpret_cast<Vector3*>(Get(OFF_LOCAL_ORIGIN));
        
        // Validate position
        if (position.x == 0 && position.y == 0 && position.z == 0) {
            valid = false;
            return;
        }
        
        // Read health and shield
        health = *reinterpret_cast<int*>(Get(OFF_HEALTH));
        maxHealth = *reinterpret_cast<int*>(Get(OFF_MAXHEALTH));
        shield = *reinterpret_cast<int*>(Get(OFF_SHIELD));
        maxShield = *reinterpret_cast<int*>(Get(OFF_MAXSHIELD));
        armorType = *reinterpret_cast<int*>(Get(OFF_ARMORTYPE)); // 0-4 for shield tier
        team = *reinterpret_cast<int*>(Get(OFF_TEAM_NUMBER));
        
        // Read velocity for prediction
        velocity = *reinterpret_cast<Vector3*>(Get(OFF_ABSVELOCITY));
        
        // Visibility check (compare lastVisibleTime with current time)
        float lastVisibleTime = *reinterpret_cast<float*>(Get(OFF_LAST_VISIBLE_TIME));
        isVisible = (lastVisibleTime > 0); // Simplified visibility check
        
        // Check life state and bleedout state
        int lifeState = *reinterpret_cast<int*>(Get(OFF_LIFE_STATE));
        bleedoutState = *reinterpret_cast<int*>(Get(OFF_BLEEDOUT_STATE));
        isKnocked = (bleedoutState > 0);
        
        // Valid if alive (lifeState == 0). We allow knocked enemies (bleedoutState > 0)
        valid = (lifeState == 0 && health > 0); 
    }
    
    void CalculateDistance(const Vector3& localPos) {
        distance = position.Distance(localPos);
    }
    
    bool IsValidForESP(const Vector3& localPos, int localTeam, float maxDistance = 25000.0f) {
        if (!valid) return false;
        
        CalculateDistance(localPos);
        
        // Distance filter
        if (distance > maxDistance) return false;
        
        // Team filter - only show enemies (DMA-style)
        if (team == localTeam) return false;
        
        return true;
    }

    Vector3 GetBonePosition(int boneIndex) {
        uintptr_t boneArray = 0;
        if (!g_Driver->ReadMemory(g_Driver->GetCurrentPid(), address + OFF_BONES, &boneArray, sizeof(boneArray)))
            return Vector3();

        // Bone matrix is 3x4 matrix (12 floats = 48 bytes)
        // Position is at offset 0x30 (x), 0x34 (y), 0x38 (z) inside the matrix?
        // Usually it's:
        // float x = matrix[0][3];
        // float y = matrix[1][3];
        // float z = matrix[2][3];
        
        struct Matrix3x4 {
            float m[3][4];
        };
        
        Matrix3x4 boneMatrix;
        uintptr_t boneAddress = boneArray + (boneIndex * sizeof(Matrix3x4));
        
        if (!g_Driver->ReadMemory(g_Driver->GetCurrentPid(), boneAddress, &boneMatrix, sizeof(boneMatrix)))
            return Vector3();
            
        return Vector3(boneMatrix.m[0][3], boneMatrix.m[1][3], boneMatrix.m[2][3]);
    }
};

class EntityScanner {
public:
    static std::vector<Entity> ScanEntities(int maxEntities = 64) {
        std::vector<Entity> entities;
        
        if (!g_Driver || !g_ApexPid || !g_ApexBase) {
            return entities;
        }

        // Limit to 64 max
        if (maxEntities > 64) maxEntities = 64;
        
        entities.reserve(maxEntities);
        
        // Entity list is at a direct offset from base
        uintptr_t entityListBase = g_ApexBase + OFF_ENTITY_LIST;
        
        // Read entity list (64 entities × 32 bytes = 2048 bytes)
        std::vector<uint8_t> entityListBuffer(maxEntities * 0x20);
        if (!g_Driver->ReadMemory(g_ApexPid, entityListBase, entityListBuffer.data(), entityListBuffer.size())) {
            return entities;
        }

        for (int i = 0; i < maxEntities; ++i) {
            if (i == 0) continue; // Skip local player at index 0

            // Get entity pointer from the buffer
            uintptr_t entityPtr = *reinterpret_cast<uintptr_t*>(&entityListBuffer[i * 0x20]);
            
            if (!entityPtr) continue;
            
            Entity entity;
            entity.index = i;
            entity.address = entityPtr;
            entity.Update();
            
            if (entity.valid) {
                entities.push_back(entity);
            }
        }
        
        return entities;
    }
};
