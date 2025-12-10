#pragma once

#ifdef _KERNEL_MODE
#include <ntdef.h>
#else
#include <Windows.h>
#include <winioctl.h>
#endif

// Use legitimate Windows Defender driver name to avoid suspicion
// Mimics Windows Defender Filter Driver (doesn't exist in Windows by default)
#define DEVICE_NAME L"\\Device\\WdFilterDrv"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\WdFilterDrv"

// Obfuscated but static GUID for shared memory
// Looks like Windows system GUID (WDF, NDIS, Storage class GUIDs use similar patterns)
// Using legitimate-looking GUID that mimics Windows Driver Framework naming
#define SHARED_SECTION_NAME L"\\BaseNamedObjects\\{1C3E4F91-8B2D-4A6E-9F7C-2E5A1D8B4F6C}"

// IOCTL codes with legitimate-looking function codes
// Mimics Windows Defender network driver patterns
#define IOCTL_WD_QUERY_INFO CTL_CODE(FILE_DEVICE_NETWORK, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WD_GET_STATISTICS CTL_CODE(FILE_DEVICE_NETWORK, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Maximum size for data transfer
#define MAX_DATA_SIZE 4096

// Command types for memory operations (READ-ONLY)
typedef enum _COMMAND_TYPE {
    CMD_NONE = 0,
    CMD_READ_MEMORY = 1,
    CMD_GET_PROCESS_BASE = 3,
    CMD_GET_DTB = 4, // Directory Table Base (CR3)
    CMD_MOUSE_CLICK = 5
} COMMAND_TYPE;

typedef enum _COMMAND_STATUS {
    CMD_STATUS_PENDING = 0,
    CMD_STATUS_COMPLETED = 1,
    CMD_STATUS_ERROR = 2
} COMMAND_STATUS;

typedef struct _MEMORY_REQUEST {
    ULONG32 ProcessId;
    unsigned __int64 Address;
    unsigned __int64 Buffer;
    ULONG32 Size;
} MEMORY_REQUEST;

typedef struct _SHARED_MEMORY {
    volatile ULONG32 Status;      // COMMAND_STATUS
    volatile ULONG32 Command;     // COMMAND_TYPE
    volatile ULONG32 Magic;       // Sanity check
    
    MEMORY_REQUEST Request;
    
    ULONG32 ResponseSize;
    UCHAR Data[MAX_DATA_SIZE];    // Buffer for read results
} SHARED_MEMORY, *PSHARED_MEMORY;
