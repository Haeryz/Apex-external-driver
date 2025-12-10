#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <random>

// Minimal process protection - no suspicious API calls
// Just provide random legitimate process names for obfuscation
class ProcessProtection {
public:
    // Get a random legitimate Windows process name
    static std::wstring GetRandomProcessName() {
        static const std::vector<std::wstring> names = {
            L"dwm.exe",                         // Desktop Window Manager
            L"RuntimeBroker.exe",               // Runtime Broker
            L"SearchApp.exe",                   // Windows Search
            L"StartMenuExperienceHost.exe",    // Start Menu
            L"TextInputHost.exe",               // Text Input
            L"ShellExperienceHost.exe",        // Shell Experience
            L"SecurityHealthSystray.exe",      // Windows Security
            L"audiodg.exe",                     // Windows Audio
            L"dasHost.exe",                     // Device Association Framework
            L"fontdrvhost.exe"                  // Usermode Font Driver Host
        };
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(names.size() - 1));
        
        return names[dis(gen)];
    }
    
    // Simple debugger check - no suspicious APIs
    static bool IsBeingDebugged() {
        return IsDebuggerPresent();
    }
    
    // NO PEB manipulation
    // NO parent process spoofing  
    // NO NtSetInformationProcess calls
    // NO handle protection tricks
    // Kernel driver provides real protection
};
