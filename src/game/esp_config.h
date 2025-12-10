#pragma once
#include "../OS-ImGui/imgui/imgui.h"

// ESP Configuration with all DMA features
struct ESPConfig {
    // Main toggles
    bool enabled = true;
    bool showBoxes = true;
    bool showHealthBars = true;
    bool showShieldBars = true;
    bool showNames = true;
    bool showDistances = true;
    bool showWeapons = false;
    bool showLevels = false;
    bool showSkeleton = false;
    bool showKnocked = true; // Show knocked state indicator
    
    // Visual settings
    float maxDistance = 500.0f; // meters
    float boxThickness = 1.5f;
    float skeletonThickness = 1.5f;
    
    // Colors
    ImVec4 enemyColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    ImVec4 teamColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
    ImVec4 visibleColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    ImVec4 knockedColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray
    
    // Shield colors by tier
    ImVec4 shieldColors[5] = {
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f), // None - Gray
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // White
        ImVec4(0.27f, 0.89f, 1.0f, 1.0f), // Blue
        ImVec4(0.88f, 0.27f, 1.0f, 1.0f), // Purple
        ImVec4(1.0f, 0.87f, 0.24f, 1.0f)  // Gold/Red
    };
    
    // Unicode obfuscation (anti-screenshot detection)
    bool useUnicodeText = true;
    
    // Screenshot protection
    bool hideOnScreenshot = true; // Clear DrawList when screenshot detected
};

// Unicode character replacements for stealth
namespace UnicodeChars {
    // Mathematical bold letters (U+1D400 series)
    inline const char* GetObfuscatedText(const char* input) {
        static std::string result;
        result.clear();
        
        // Simple character-by-character replacement
        // E → 𝐄, S → 𝐒, P → 𝐏, etc.
        for (const char* p = input; *p; ++p) {
            switch (*p) {
                case 'E': result += u8"𝐄"; break;
                case 'S': result += u8"𝐒"; break;
                case 'P': result += u8"𝐏"; break;
                case 'H': result += u8"𝐇"; break;
                case 'e': result += u8"𝐞"; break;
                case 's': result += u8"𝐬"; break;
                case 'p': result += u8"𝐩"; break;
                case 'h': result += u8"𝐡"; break;
                case 'a': result += u8"𝐚"; break;
                case 'l': result += u8"𝐥"; break;
                case 't': result += u8"𝐭"; break;
                default: result += *p; break;
            }
        }
        
        return result.c_str();
    }
    
    // Alternative: Box drawing characters for more obfuscation
    inline const char* Health() { return u8"━━━ ᕼ 󠀀"; } // Health with special spacing
    inline const char* Shield() { return u8"━━━ 󠀀"; } // Shield with special char
    inline const char* Distance() { return u8""; } // Meter symbol
}
