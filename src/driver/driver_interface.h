#pragma once
#include <Windows.h>
#include "common.h"

class DriverInterface {
private:
    HANDLE deviceHandle;
    PSHARED_MEMORY sharedMemory;
    HANDLE sectionHandle;
    DWORD currentPid;
    
public:
    DriverInterface();
    ~DriverInterface();
    
    bool Initialize();
    void Cleanup();
    
    DWORD GetProcessId(const wchar_t* processName);
    uintptr_t GetModuleBase(DWORD pid, const wchar_t* moduleName);
    uintptr_t GetCR3(DWORD pid);
    bool ReadMemory(DWORD processId, uintptr_t address, void* buffer, size_t size);
    
    // Template helper for easy read (write removed - ESP read-only mode)
    template<typename T>
    T Read(uintptr_t address) {
        T value{};
        ReadMemory(currentPid, address, &value, sizeof(T));
        return value;
    }
    
    // Mouse injection (kernel-level via bootkit-protected driver)
    bool InjectMouseClick();
    
    // Process protection - enables ObCallbacks to protect this process
    bool EnableProcessProtection();
    bool DisableProcessProtection();
    
    void SetCurrentPid(DWORD pid) { currentPid = pid; }
    DWORD GetCurrentPid() const { return currentPid; }
    
private:
    bool SendCommand(COMMAND_TYPE command);
    bool WaitForCompletion(DWORD timeoutMs = 50);
};
