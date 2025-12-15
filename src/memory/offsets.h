#pragma once
#include <cstdint>


// === Core Game Pointers (from offsets.txt 12/15/2025) ===
#define OFF_LOCAL_PLAYER 0x2a7bec8       // LocalPlayer
#define OFF_ENTITY_LIST 0x65fe858        // EntityList
#define OFF_VIEWRENDER 0x40d8bb8         // ViewRender
#define OFF_VIEWMATRIX 0x11a350          // ViewMatrix offset from ViewRender
#define OFF_NAME_LIST 0x9039240          // NameList

// === Player & Entity Stats (from DT_Player / offsets.txt) ===
#define OFF_HEALTH 0x0324                // m_iHealth
#define OFF_MAXHEALTH 0x0468             // m_iMaxHealth  
#define OFF_SHIELD 0x0170                // m_shieldHealth
#define OFF_MAXSHIELD 0x0174             // m_shieldHealthMax
#define OFF_ARMORTYPE 0x4974             // m_armorType (from DT_Player)
#define OFF_LOCAL_ORIGIN 0x017c          // m_vecAbsOrigin - ACTUAL position vector
#define OFF_TEAM_NUMBER 0x0334           // m_iTeamNum
#define OFF_LIFE_STATE 0x0690            // m_lifeState (0 = alive)
#define OFF_BLEEDOUT_STATE 0x2920        // m_bleedoutState (from DT_Player)
#define OFF_LAST_VISIBLE_TIME 0x1a54     // lastVisibleTime

// === Bone System (for skeleton ESP if needed) ===
#define OFFSET_STUDIOHDR 0xff0           // studioHdr
#define OFF_BONES 0x0db8                 // m_nForceBone + offset

// === Prediction & Movement (for TriggerBot) ===
#define OFF_ABSVELOCITY 0x0170           // m_vecAbsVelocity
#define OFF_WEAPON_HANDLE 0x19b4         // m_latestPrimaryWeapons
#define OFF_WEAPON_INDEX 0x1818          // Weapon name index
#define OFF_PROJECTILESPEED 0x1ed0       // m_flProjectileSpeed
#define OFF_PROJECTILESCALE 0x1ed8       // m_flProjectileScale
#define OFF_LAST_AIMEDAT_TIME 0x1a5c     // lastCrosshairTargetTime

// === Camera (for view angles) ===
#define OFF_CAMERA_ORIGIN 0x1fb4         // camera_origin
#define OFF_CAMERA_ANGLES 0x1fc0         // camera_angles