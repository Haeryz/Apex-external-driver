#pragma once
#include <cstdint>


// === Core Game Pointers ===
#define OFF_LOCAL_PLAYER 0x2a7be28       // Local player pointer
#define OFF_ENTITY_LIST 0x65fe6d8        // Entity list base
#define OFF_VIEWRENDER 0x40d8a38         // ViewRender pointer
#define OFF_VIEWMATRIX 0x11a350          // ViewMatrix offset from ViewRender

// === Player & Entity Stats (ESP Critical) ===
#define OFF_HEALTH 0x0324                // Current health
#define OFF_MAXHEALTH 0x0468             // Maximum health
#define OFF_SHIELD 0x01a0                // Current shield
#define OFF_MAXSHIELD 0x01A4             // Maximum shield
#define OFF_ARMORTYPE 0x4974             // Armor type (0=None, 1=White, 2=Blue, 3=Purple, 4=Gold)
#define OFF_LOCAL_ORIGIN 0x017c          // Entity position (X, Y, Z)
#define OFF_TEAM_NUMBER 0x0334           // Team ID
#define OFF_LIFE_STATE 0x0690            // Life state (0 = alive)
#define OFF_BLEEDOUT_STATE 0x2920        // Bleedout/knockdown state (0 = not knocked)
#define OFF_LAST_VISIBLE_TIME 0x1a54     // Last time entity was visible

// === Bone System (for skeleton ESP if needed) ===
#define OFFSET_STUDIOHDR 0xff0           // Studio model header
#define OFF_BONES (0xda8 + 0x48)         // Bone matrix base

// === Prediction & Movement (for TriggerBot) ===
#define OFF_ABSVELOCITY 0x0170           // Absolute velocity (Vec3)
#define OFF_WEAPON_HANDLE 0x19b4         // Latest primary weapon handle
#define OFF_WEAPON_INDEX 0x1818          // Weapon name index
#define OFF_PROJECTILESPEED 0x2820       // Projectile launch speed
#define OFF_PROJECTILESCALE (OFF_PROJECTILESPEED + 0x8)       // Projectile gravity scale (OFF_PROJECTILESPEED + 0x8)
#define OFF_LAST_AIMEDAT_TIME 0x1a5c     // Last crosshair target time

