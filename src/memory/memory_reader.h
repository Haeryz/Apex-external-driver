#pragma once
#include "../driver/driver_interface.h"

// Global driver instance
extern DriverInterface* g_Driver;
extern DWORD g_ApexPid;
extern uintptr_t g_ApexBase;

// Template for reading memory
template<typename T>
inline T Read(uintptr_t address) {
    T value{};
    if (g_Driver && g_ApexPid) {
        g_Driver->ReadMemory(g_ApexPid, address, &value, sizeof(T));
    }
    return value;
}

// Specialized template for reading arrays
template<typename T>
inline bool ReadArray(uintptr_t address, T* buffer, size_t count) {
    if (g_Driver && g_ApexPid) {
        return g_Driver->ReadMemory(g_ApexPid, address, buffer, sizeof(T) * count);
    }
    return false;
}
