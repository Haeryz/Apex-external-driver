#include "driver_interface.h"
#include <iostream>
#include <TlHelp32.h>
#include <emmintrin.h>

// IOCTL codes already defined in common.h (included via driver_interface.h)

DriverInterface::DriverInterface() : deviceHandle(INVALID_HANDLE_VALUE), sectionHandle(NULL), sharedMemory(nullptr), currentPid(0) {
}

DriverInterface::~DriverInterface() {
    Cleanup();
}

bool DriverInterface::Initialize() {
    std::cout << "[*] Step 1: Opening driver handle..." << std::endl;
    
    // 1. Open Driver Handle (with retry for bootkit initialization)
    int retries = 10; // Reduced to 10 seconds for faster feedback
    while (retries > 0) {
        deviceHandle = CreateFileW(L"\\\\.\\WdFilterDrv", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        
        if (deviceHandle != INVALID_HANDLE_VALUE) {
            std::cout << "[+] Driver handle opened!" << std::endl;
            break; // Success
        }
        
        DWORD err = GetLastError();
        std::cout << "[-] CreateFile failed (error " << err << "), retrying... " << retries << "s left" << std::endl;
        Sleep(1000); // Wait 1 second before retry
        retries--;
    }
    
    if (deviceHandle == INVALID_HANDLE_VALUE) {
        std::cout << "[-] Failed to open driver device after retries" << std::endl;
        std::cout << "    Make sure driver is loaded with kdmapper!" << std::endl;
        return false;
    }
    
    std::cout << "[*] Step 2: Triggering shared memory creation..." << std::endl;
    
    // 2. Trigger Shared Memory Creation (using legitimate IOCTL name)
    DWORD bytes = 0;
    if (!DeviceIoControl(deviceHandle, IOCTL_WD_GET_STATISTICS, NULL, 0, NULL, 0, &bytes, NULL)) {
        DWORD err = GetLastError();
        std::cout << "[-] DeviceIoControl failed (error " << err << ")" << std::endl;
        CloseHandle(deviceHandle);
        deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    std::cout << "[+] IOCTL sent successfully!" << std::endl;
    
    std::cout << "[*] Step 3: Opening shared memory..." << std::endl;
    
    // 3. Open Shared Memory Mapping (using obfuscated GUID)
    // GUID mimics legitimate Windows Driver Framework patterns
    sectionHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Global\\{1C3E4F91-8B2D-4A6E-9F7C-2E5A1D8B4F6C}");
    if (!sectionHandle) {
        DWORD err1 = GetLastError();
        std::cout << "[-] OpenFileMapping (Global) failed (error " << err1 << "), trying without Global..." << std::endl;
        sectionHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"{1C3E4F91-8B2D-4A6E-9F7C-2E5A1D8B4F6C}");
    }
    
    if (!sectionHandle) {
        DWORD err2 = GetLastError();
        std::cout << "[-] OpenFileMapping failed (error " << err2 << ")" << std::endl;
        std::cout << "    Shared memory section not found - driver may not have created it" << std::endl;
        CloseHandle(deviceHandle);
        deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    std::cout << "[+] Shared memory section opened!" << std::endl;
    
    std::cout << "[*] Step 4: Mapping shared memory view..." << std::endl;
    
    sharedMemory = (PSHARED_MEMORY)MapViewOfFile(sectionHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_MEMORY));
    if (!sharedMemory) {
        DWORD err = GetLastError();
        std::cout << "[-] MapViewOfFile failed (error " << err << ")" << std::endl;
        CloseHandle(sectionHandle);
        CloseHandle(deviceHandle);
        sectionHandle = NULL;
        deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }
    
    std::cout << "[+] Driver initialized successfully!" << std::endl;
    return true;
}

bool DriverInterface::SendCommand(COMMAND_TYPE command) {
    if (!sharedMemory || !deviceHandle || deviceHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    sharedMemory->Command = command;
    sharedMemory->Status = CMD_STATUS_PENDING;
    
    // Signal driver (using legitimate IOCTL name)
    DWORD bytes = 0;
    if (!DeviceIoControl(deviceHandle, IOCTL_WD_QUERY_INFO, NULL, 0, NULL, 0, &bytes, NULL)) {
        return false;
    }
    
    // Spin-wait for completion (no sleep for max speed)
    int timeout = 10000;  // ~10000 spins max
    while (sharedMemory->Status == CMD_STATUS_PENDING && timeout-- > 0) {
        _mm_pause();  // CPU hint for spin-wait
    }
    
    return (sharedMemory->Status == CMD_STATUS_COMPLETED);
}

bool DriverInterface::WaitForCompletion(DWORD timeoutMs) {
    if (!sharedMemory) return false;
    
    DWORD attempts = timeoutMs / 10;
    while (sharedMemory->Status == CMD_STATUS_PENDING && attempts > 0) {
        Sleep(10);
        attempts--;
    }
    
    return (sharedMemory->Status == CMD_STATUS_COMPLETED);
}

DWORD DriverInterface::GetProcessId(const wchar_t* processName) {
    PROCESSENTRY32W pt;
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pt.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hsnap, &pt)) {
        do {
            if (_wcsicmp(pt.szExeFile, processName) == 0) {
                CloseHandle(hsnap);
                return pt.th32ProcessID;
            }
        } while (Process32NextW(hsnap, &pt));
    }
    
    CloseHandle(hsnap);
    return 0;
}

uintptr_t DriverInterface::GetModuleBase(DWORD processId, const wchar_t* moduleName) {
    if (!sharedMemory) return 0;
    
    sharedMemory->Request.ProcessId = processId;
    if (SendCommand(CMD_GET_PROCESS_BASE)) {
        return *(uintptr_t*)sharedMemory->Data;
    }
    std::cerr << "[-] CMD_GET_PROCESS_BASE failed" << std::endl;
    return 0;
}

uintptr_t DriverInterface::GetCR3(DWORD processId) {
    if (!sharedMemory) return 0;
    
    sharedMemory->Request.ProcessId = processId;
    if (SendCommand(CMD_GET_DTB)) {
        return *(uintptr_t*)sharedMemory->Data;
    }
    std::cerr << "[-] CMD_GET_DTB failed" << std::endl;
    return 0;
}

bool DriverInterface::ReadMemory(DWORD processId, uintptr_t address, void* buffer, size_t size) {
    if (!sharedMemory) return false;
    
    // Handle chunking if size > MAX_DATA_SIZE
    BYTE* dest = (BYTE*)buffer;
    DWORD remaining = (DWORD)size;
    DWORD64 current_addr = (DWORD64)address;
    
    while (remaining > 0) {
        DWORD chunk_size = (remaining > MAX_DATA_SIZE) ? MAX_DATA_SIZE : remaining;
        
        sharedMemory->Request.ProcessId = processId;
        sharedMemory->Request.Address = current_addr;
        sharedMemory->Request.Size = chunk_size;
        
        if (SendCommand(CMD_READ_MEMORY)) {
            memcpy(dest, sharedMemory->Data, chunk_size);
        } else {
            memset(dest, 0, chunk_size); // Failed
            return false;
        }
        
        remaining -= chunk_size;
        current_addr += chunk_size;
        dest += chunk_size;
    }
    
    return true;
}

bool DriverInterface::InjectMouseClick() {
    if (!sharedMemory) {
        std::cerr << "[-] InjectMouseClick: sharedMemory is NULL" << std::endl;
        return false;
    }
    
    if (deviceHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] InjectMouseClick: deviceHandle is invalid" << std::endl;
        return false;
    }
    
    // Send mouse click command to kernel driver (HID raw report injection)
    std::cout << "[DEBUG] Sending kernel HID injection command..." << std::endl;
    bool success = SendCommand(CMD_MOUSE_CLICK);
    
    if (!success) {
        std::cerr << "[-] Kernel mouse injection failed" << std::endl;
        std::cerr << "[-] SharedMemory Status: " << (int)sharedMemory->Status << std::endl;
        
        // Decode the error if driver returned one
        if (sharedMemory->Status == CMD_STATUS_ERROR && sharedMemory->ResponseSize >= sizeof(ULONG32)) {
            ULONG32 clickStatus = *(ULONG32*)sharedMemory->Data;
            std::cerr << "[-] Driver reported error: 0x" << std::hex << std::uppercase << clickStatus << std::dec << std::endl;

            // Decode common errors
            if (clickStatus == 0xC00000BB) std::cerr << "    = STATUS_NOT_SUPPORTED" << std::endl;
            else if (clickStatus == 0xC00000C0) std::cerr << "    = STATUS_DEVICE_NOT_READY" << std::endl;
            else if (clickStatus == 0xC0000001) std::cerr << "    = STATUS_UNSUCCESSFUL" << std::endl;
            else if (clickStatus == 0xC0000002) std::cerr << "    = STATUS_NOT_IMPLEMENTED" << std::endl;
            else if (clickStatus == 0xC000000D) std::cerr << "    = STATUS_INVALID_PARAMETER" << std::endl;
            else if (clickStatus == 0xC00000A3) std::cerr << "    = STATUS_DEVICE_ALREADY_ATTACHED (filter attach failed)" << std::endl;
            
            // If driver returned extended diagnostics, print them
            if (sharedMemory->ResponseSize >= (sizeof(ULONG32) * 2)) {
                ULONG32 detail = *(ULONG32*)(sharedMemory->Data + sizeof(ULONG32));
                std::cerr << "[-] Driver detail DWORD: 0x" << std::hex << std::uppercase << detail << std::dec << std::endl;
                
                // Decode special markers and flags
                if (detail == 0xDEADBEEF) {
                    std::cerr << "    = MARKER: No lower device available (g_LowerMouseDevice is NULL)" << std::endl;
                    std::cerr << "    = This means InitializeMouseDevice() failed to set up the device" << std::endl;
                } else {
                    std::cerr << "    - Interpretation:" << std::endl;
                    if (detail & 0x80000000) std::cerr << "      * bit 31: MouseClassServiceCallback is present" << std::endl;
                    if (detail & 0x40000000) std::cerr << "      * bit 30: Callback-based injection was attempted" << std::endl;
                    if (detail & 0x20000000) std::cerr << "      * bit 29: Exception occurred in callback" << std::endl;
                    if (detail & 0x10000000) std::cerr << "      * bit 28: Using class device (not mouse device)" << std::endl;
                    if (detail & 0x08000000) std::cerr << "      * bit 27: Callback address validation failed" << std::endl;
                    if ((detail & 0xFFFF) != 0) {
                        std::cerr << "      * lower 16 bits: 0x" << std::hex << (detail & 0xFFFF) << std::dec 
                                  << " (IRP down status or exception code)" << std::endl;
                    }
                    if (((detail >> 16) & 0xFF) != 0) {
                        std::cerr << "      * bits 16-23: 0x" << std::hex << ((detail >> 16) & 0xFF) << std::dec 
                                  << " (IRP up status)" << std::endl;
                    }
                }
            }
        }
        
        return false;
    }
    
    std::cout << "[+] Kernel mouse injection successful!" << std::endl;
    return true;
}

bool DriverInterface::EnableProcessProtection() {
    if (!sharedMemory || deviceHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Get our own PID and send it to the driver for protection
    DWORD myPid = GetCurrentProcessId();
    
    sharedMemory->Request.ProcessId = myPid;
    sharedMemory->Request.Address = 0;
    sharedMemory->Request.Buffer = 0;
    sharedMemory->Request.Size = 0;
    
    bool success = SendCommand(CMD_SET_PROTECTED_PID);
    
    if (success) {
        std::cout << "[+] Process protection enabled for PID " << myPid << std::endl;
    }
    
    return success;
}

bool DriverInterface::DisableProcessProtection() {
    if (!sharedMemory || deviceHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Send PID 0 to disable protection
    sharedMemory->Request.ProcessId = 0;
    sharedMemory->Request.Address = 0;
    sharedMemory->Request.Buffer = 0;
    sharedMemory->Request.Size = 0;
    
    bool success = SendCommand(CMD_SET_PROTECTED_PID);
    
    if (success) {
        std::cout << "[+] Process protection disabled" << std::endl;
    }
    
    return success;
}

void DriverInterface::Cleanup() {
    if (sharedMemory) {
        UnmapViewOfFile(sharedMemory);
        sharedMemory = nullptr;
    }
    
    if (sectionHandle) {
        CloseHandle(sectionHandle);
        sectionHandle = NULL;
    }
    
    if (deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(deviceHandle);
        deviceHandle = INVALID_HANDLE_VALUE;
    }
}
