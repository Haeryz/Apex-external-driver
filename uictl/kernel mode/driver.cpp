#include <ntifs.h>
#include <windef.h>

// ============================================================================
// KDMAPPER-COMPATIBLE DRIVER with IoCreateDriver
// Fixed version with proper error handling and safe memory operations
// NOW WITH ObRegisterCallbacks PROCESS PROTECTION
// ============================================================================

// Process access rights (not defined in kernel headers)
#ifndef PROCESS_VM_READ
#define PROCESS_VM_READ           0x0010
#define PROCESS_VM_WRITE          0x0020
#define PROCESS_VM_OPERATION      0x0008
#define PROCESS_DUP_HANDLE        0x0040
#define PROCESS_CREATE_THREAD     0x0002
#define PROCESS_TERMINATE         0x0001
#define PROCESS_SUSPEND_RESUME    0x0800
#endif

#ifndef THREAD_SUSPEND_RESUME
#define THREAD_SUSPEND_RESUME     0x0002
#define THREAD_TERMINATE          0x0001
#define THREAD_SET_CONTEXT        0x0010
#define THREAD_GET_CONTEXT        0x0008
#endif

PDEVICE_OBJECT g_DeviceObject = NULL;
HANDLE g_SectionHandle = NULL;
PVOID g_SharedMemory = NULL;
volatile LONG g_DriverReady = 0;  // Flag to indicate driver is ready

// ============================================================================
// PROCESS PROTECTION - ObRegisterCallbacks
// ============================================================================

PVOID g_CallbackRegistration = NULL;  // Handle to unregister callbacks
ULONG g_ProtectedPid = 0;             // PID to protect (set by ext.exe)

// Command to set protected PID
#define CMD_SET_PROTECTED_PID 10

// Pre-operation callback for process handle operations
OB_PREOP_CALLBACK_STATUS PreProcessHandleCallback(
    PVOID RegistrationContext,
    POB_PRE_OPERATION_INFORMATION OperationInfo
) {
    UNREFERENCED_PARAMETER(RegistrationContext);
    
    __try {
        if (!OperationInfo || !OperationInfo->Object) {
            return OB_PREOP_SUCCESS;
        }
        
        // Only protect if we have a PID to protect
        if (g_ProtectedPid == 0) {
            return OB_PREOP_SUCCESS;
        }
        
        PEPROCESS targetProcess = (PEPROCESS)OperationInfo->Object;
        HANDLE targetPid = PsGetProcessId(targetProcess);
        
        // If someone is opening a handle to our protected process
        if ((ULONG)(ULONG_PTR)targetPid == g_ProtectedPid) {
            // Don't strip our own access
            PEPROCESS currentProcess = PsGetCurrentProcess();
            HANDLE currentPid = PsGetProcessId(currentProcess);
            
            if ((ULONG)(ULONG_PTR)currentPid == g_ProtectedPid) {
                return OB_PREOP_SUCCESS;  // Allow self-access
            }
            
            // Allow SYSTEM (PID 4) and csrss
            if ((ULONG_PTR)currentPid <= 4) {
                return OB_PREOP_SUCCESS;
            }
            
            // Strip dangerous access rights from everyone else
            if (OperationInfo->Operation == OB_OPERATION_HANDLE_CREATE) {
                // Remove rights that allow memory read/write and process manipulation
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_OPERATION;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_DUP_HANDLE;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_CREATE_THREAD;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_SUSPEND_RESUME;
            }
            else if (OperationInfo->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_OPERATION;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_DUP_HANDLE;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_CREATE_THREAD;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_SUSPEND_RESUME;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent failure
    }
    
    return OB_PREOP_SUCCESS;
}

// Pre-operation callback for thread handle operations
OB_PREOP_CALLBACK_STATUS PreThreadHandleCallback(
    PVOID RegistrationContext,
    POB_PRE_OPERATION_INFORMATION OperationInfo
) {
    UNREFERENCED_PARAMETER(RegistrationContext);
    
    __try {
        if (!OperationInfo || !OperationInfo->Object) {
            return OB_PREOP_SUCCESS;
        }
        
        if (g_ProtectedPid == 0) {
            return OB_PREOP_SUCCESS;
        }
        
        PETHREAD targetThread = (PETHREAD)OperationInfo->Object;
        PEPROCESS ownerProcess = IoThreadToProcess(targetThread);
        
        if (!ownerProcess) {
            return OB_PREOP_SUCCESS;
        }
        
        HANDLE ownerPid = PsGetProcessId(ownerProcess);
        
        // If thread belongs to our protected process
        if ((ULONG)(ULONG_PTR)ownerPid == g_ProtectedPid) {
            PEPROCESS currentProcess = PsGetCurrentProcess();
            HANDLE currentPid = PsGetProcessId(currentProcess);
            
            if ((ULONG)(ULONG_PTR)currentPid == g_ProtectedPid) {
                return OB_PREOP_SUCCESS;  // Allow self-access
            }
            
            if ((ULONG_PTR)currentPid <= 4) {
                return OB_PREOP_SUCCESS;  // Allow SYSTEM
            }
            
            // Strip dangerous thread access rights
            if (OperationInfo->Operation == OB_OPERATION_HANDLE_CREATE) {
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_TERMINATE;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_SET_CONTEXT;
                OperationInfo->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_GET_CONTEXT;
            }
            else if (OperationInfo->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_TERMINATE;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_SET_CONTEXT;
                OperationInfo->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_GET_CONTEXT;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent failure
    }
    
    return OB_PREOP_SUCCESS;
}

// Register ObCallbacks for process/thread protection
NTSTATUS RegisterProtectionCallbacks() {
    __try {
        if (g_CallbackRegistration != NULL) {
            return STATUS_SUCCESS;  // Already registered
        }
        
        OB_OPERATION_REGISTRATION opRegistration[2] = { 0 };
        OB_CALLBACK_REGISTRATION callbackRegistration = { 0 };
        
        // Process handle callback
        opRegistration[0].ObjectType = PsProcessType;
        opRegistration[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
        opRegistration[0].PreOperation = PreProcessHandleCallback;
        opRegistration[0].PostOperation = NULL;
        
        // Thread handle callback
        opRegistration[1].ObjectType = PsThreadType;
        opRegistration[1].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
        opRegistration[1].PreOperation = PreThreadHandleCallback;
        opRegistration[1].PostOperation = NULL;
        
        // Callback registration structure
        callbackRegistration.Version = OB_FLT_REGISTRATION_VERSION;
        callbackRegistration.OperationRegistrationCount = 2;
        callbackRegistration.OperationRegistration = opRegistration;
        
        // Use a fake altitude for stealth (looks like AV driver)
        RtlInitUnicodeString(&callbackRegistration.Altitude, L"321000");
        callbackRegistration.RegistrationContext = NULL;
        
        NTSTATUS status = ObRegisterCallbacks(&callbackRegistration, &g_CallbackRegistration);
        return status;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

void UnregisterProtectionCallbacks() {
    __try {
        if (g_CallbackRegistration != NULL) {
            ObUnRegisterCallbacks(g_CallbackRegistration);
            g_CallbackRegistration = NULL;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent failure
    }
}

// Windows version offsets
#define win_1803 17134
#define win_1809 17763
#define win_1903 18362
#define win_1909 18363
#define win_2004 19041
#define win_20H2 19569
#define win_21H1 20180

#define PAGE_OFFSET_SIZE 12
static const UINT64 PMASK = (~0xfull << 8) & 0xfffffffffull;

// IOCTL codes matching ext.exe
#define IOCTL_WD_QUERY_INFO CTL_CODE(FILE_DEVICE_NETWORK, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WD_GET_STATISTICS CTL_CODE(FILE_DEVICE_NETWORK, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_DATA_SIZE 4096

typedef enum _COMMAND_TYPE {
    CMD_NONE = 0,
    CMD_READ_MEMORY = 1,
    CMD_GET_PROCESS_BASE = 3,
    CMD_GET_DTB = 4,
    CMD_MOUSE_CLICK = 5
} COMMAND_TYPE;

typedef enum _COMMAND_STATUS {
    CMD_STATUS_PENDING = 0,
    CMD_STATUS_COMPLETED = 1,
    CMD_STATUS_ERROR = 2
} COMMAND_STATUS;

typedef struct _MEMORY_REQUEST {
    ULONG32 ProcessId;
    ULONGLONG Address;
    ULONGLONG Buffer;
    ULONG32 Size;
} MEMORY_REQUEST;

typedef struct _SHARED_MEMORY {
    volatile ULONG32 Status;
    volatile ULONG32 Command;
    volatile ULONG32 Magic;
    MEMORY_REQUEST Request;
    ULONG32 ResponseSize;
    UCHAR Data[MAX_DATA_SIZE];
} SHARED_MEMORY, *PSHARED_MEMORY;

// Forward declaration
NTSTATUS RegisterProtectionCallbacks();

// Handle command to set protected PID
NTSTATUS HandleSetProtectedPid(PSHARED_MEMORY sharedMem) {
    __try {
        if (!sharedMem) return STATUS_INVALID_PARAMETER;
        
        // The PID to protect is passed in Request.ProcessId
        ULONG newPid = sharedMem->Request.ProcessId;
        
        if (newPid == 0) {
            // Disable protection
            g_ProtectedPid = 0;
            return STATUS_SUCCESS;
        }
        
        // Verify the process exists
        PEPROCESS process = NULL;
        NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)newPid, &process);
        if (!NT_SUCCESS(status) || !process) {
            return STATUS_NOT_FOUND;
        }
        ObDereferenceObject(process);
        
        // Set the protected PID
        g_ProtectedPid = newPid;
        
        // Make sure callbacks are registered
        if (g_CallbackRegistration == NULL) {
            RegisterProtectionCallbacks();
        }
        
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

extern "C" NTSTATUS NTAPI IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);
extern "C" PVOID NTAPI PsGetProcessSectionBaseAddress(PEPROCESS Process);

// Safe wrapper to check if address is valid for kernel access
BOOLEAN IsAddressValid(PVOID Address) {
    if (Address == NULL) return FALSE;
    if ((UINT64)Address < 0x1000) return FALSE;
    return MmIsAddressValid(Address);
}

// ============================================================================
// Physical memory functions (with safe wrappers)
// ============================================================================

NTSTATUS SafeReadPhysicalMemory(UINT64 physAddress, PVOID buffer, SIZE_T size, SIZE_T* bytesRead) {
    __try {
        if (bytesRead) *bytesRead = 0;
        
        if (physAddress == 0 || buffer == NULL || size == 0) {
            return STATUS_INVALID_PARAMETER;
        }
        
        // Validate physical address range (typical RAM limit: 128GB = 0x2000000000)
        if (physAddress > 0x2000000000ULL) {
            return STATUS_INVALID_ADDRESS;
        }
        
        // Don't read more than 1MB at once
        if (size > 0x100000) {
            return STATUS_INVALID_PARAMETER;
        }
        
        MM_COPY_ADDRESS toRead = { 0 };
        toRead.PhysicalAddress.QuadPart = (LONGLONG)physAddress;
        return MmCopyMemory(buffer, toRead, size, MM_COPY_MEMORY_PHYSICAL, bytesRead);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (bytesRead) *bytesRead = 0;
        return STATUS_ACCESS_VIOLATION;
    }
}

INT32 GetWindowsVersion() {
    __try {
        RTL_OSVERSIONINFOW ver = { 0 };
        ver.dwOSVersionInfoSize = sizeof(ver);
        RtlGetVersion(&ver);
        switch (ver.dwBuildNumber) {
        case win_1803:
        case win_1809:
            return 0x0278;
        case win_1903:
        case win_1909:
            return 0x0280;
        default:
            return 0x0388;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0x0388; // Default for modern Windows
    }
}

UINT64 GetProcessCr3(PEPROCESS pProcess) {
    __try {
        if (!pProcess || !IsAddressValid(pProcess)) {
            return 0;
        }
        
        PUCHAR process = (PUCHAR)pProcess;
        
        // Check if we can read the DirectoryTableBase at offset 0x28
        if (!IsAddressValid(process + 0x28)) {
            return 0;
        }
        
        ULONG_PTR processDirbase = *(PULONG_PTR)(process + 0x28);
        if (processDirbase == 0) {
            INT32 userDirOffset = GetWindowsVersion();
            if (!IsAddressValid(process + userDirOffset)) {
                return 0;
            }
            return *(PULONG_PTR)(process + userDirOffset);
        }
        return processDirbase;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

UINT64 TranslateLinear(UINT64 directoryTableBase, UINT64 virtualAddress) {
    __try {
        if (directoryTableBase == 0 || virtualAddress == 0) {
            return 0;
        }
        
        // Validate virtual address is canonical
        if (virtualAddress > 0x00007FFFFFFFFFFF && virtualAddress < 0xFFFF800000000000) {
            return 0; // Non-canonical address
        }
        
        directoryTableBase &= ~0xf;
        UINT64 pageOffset = virtualAddress & ~(~0ul << PAGE_OFFSET_SIZE);
        UINT64 pte = ((virtualAddress >> 12) & (0x1ffll));
        UINT64 pt = ((virtualAddress >> 21) & (0x1ffll));
        UINT64 pd = ((virtualAddress >> 30) & (0x1ffll));
        UINT64 pdp = ((virtualAddress >> 39) & (0x1ffll));

        SIZE_T readsize = 0;
        UINT64 pdpe = 0;
        
        UINT64 pdpAddr = directoryTableBase + 8 * pdp;
        if (pdpAddr > 0x2000000000ULL) return 0; // Physical address out of range
        
        NTSTATUS status = SafeReadPhysicalMemory(pdpAddr, &pdpe, sizeof(pdpe), &readsize);
        if (!NT_SUCCESS(status) || (~pdpe & 1)) return 0;

        UINT64 pde = 0;
        UINT64 pdAddr = (pdpe & PMASK) + 8 * pd;
        if (pdAddr > 0x2000000000ULL) return 0;
        
        status = SafeReadPhysicalMemory(pdAddr, &pde, sizeof(pde), &readsize);
        if (!NT_SUCCESS(status) || (~pde & 1)) return 0;

        if (pde & 0x80)
            return (pde & (~0ull << 42 >> 12)) + (virtualAddress & ~(~0ull << 30));

        UINT64 pteAddr = 0;
        UINT64 ptAddr = (pde & PMASK) + 8 * pt;
        if (ptAddr > 0x2000000000ULL) return 0;
        
        status = SafeReadPhysicalMemory(ptAddr, &pteAddr, sizeof(pteAddr), &readsize);
        if (!NT_SUCCESS(status) || (~pteAddr & 1)) return 0;

        if (pteAddr & 0x80)
            return (pteAddr & PMASK) + (virtualAddress & ~(~0ull << 21));

        UINT64 addr = 0;
        UINT64 finalAddr = (pteAddr & PMASK) + 8 * pte;
        if (finalAddr > 0x2000000000ULL) return 0;
        
        status = SafeReadPhysicalMemory(finalAddr, &addr, sizeof(addr), &readsize);
        if (!NT_SUCCESS(status)) return 0;
        
        addr &= PMASK;
        if (!addr) return 0;
        
        UINT64 result = addr + pageOffset;
        if (result > 0x2000000000ULL) return 0; // Final physical address out of range
        
        return result;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

ULONG64 FindMin(INT32 g, SIZE_T f) {
    return (ULONG64)(g < (INT32)f ? g : (INT32)f);
}

// ============================================================================
// Command handlers (with safe wrappers)
// ============================================================================

NTSTATUS HandleReadMemory(PSHARED_MEMORY sharedMem) {
    __try {
        if (!sharedMem) return STATUS_INVALID_PARAMETER;
        if (!sharedMem->Request.ProcessId || !sharedMem->Request.Address || !sharedMem->Request.Size)
            return STATUS_INVALID_PARAMETER;
        if (sharedMem->Request.Size > MAX_DATA_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        // Validate virtual address is canonical
        UINT64 vAddr = sharedMem->Request.Address;
        if (vAddr > 0x00007FFFFFFFFFFF && vAddr < 0xFFFF800000000000) {
            return STATUS_INVALID_ADDRESS; // Non-canonical address
        }

        PEPROCESS process = NULL;
        NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)sharedMem->Request.ProcessId, &process);
        if (!NT_SUCCESS(status) || !process)
            return STATUS_NOT_FOUND;

        UINT64 cr3 = GetProcessCr3(process);
        ObDereferenceObject(process);
        
        if (!cr3) return STATUS_UNSUCCESSFUL;

        UINT64 physAddr = TranslateLinear(cr3, vAddr);
        if (!physAddr) return STATUS_UNSUCCESSFUL;

        SIZE_T bytesRead = 0;
        ULONG64 sizeToRead = FindMin(PAGE_SIZE - (physAddr & 0xFFF), sharedMem->Request.Size);
        
        // Additional safety check
        if (sizeToRead == 0 || sizeToRead > MAX_DATA_SIZE) {
            return STATUS_INVALID_PARAMETER;
        }
        
        status = SafeReadPhysicalMemory(physAddr, sharedMem->Data, (SIZE_T)sizeToRead, &bytesRead);
        if (NT_SUCCESS(status)) {
            sharedMem->ResponseSize = (ULONG32)bytesRead;
        }
        return status;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

NTSTATUS HandleGetProcessBase(PSHARED_MEMORY sharedMem) {
    __try {
        if (!sharedMem) return STATUS_INVALID_PARAMETER;
        if (!sharedMem->Request.ProcessId)
            return STATUS_INVALID_PARAMETER;

        PEPROCESS process = NULL;
        NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)sharedMem->Request.ProcessId, &process);
        if (!NT_SUCCESS(status) || !process)
            return STATUS_NOT_FOUND;

        ULONGLONG imageBase = (ULONGLONG)PsGetProcessSectionBaseAddress(process);
        ObDereferenceObject(process);
        
        if (!imageBase) return STATUS_UNSUCCESSFUL;

        RtlCopyMemory(sharedMem->Data, &imageBase, sizeof(imageBase));
        sharedMem->ResponseSize = sizeof(imageBase);
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

NTSTATUS HandleGetDtb(PSHARED_MEMORY sharedMem) {
    __try {
        if (!sharedMem) return STATUS_INVALID_PARAMETER;
        if (!sharedMem->Request.ProcessId)
            return STATUS_INVALID_PARAMETER;

        PEPROCESS process = NULL;
        NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)sharedMem->Request.ProcessId, &process);
        if (!NT_SUCCESS(status) || !process)
            return STATUS_NOT_FOUND;

        UINT64 cr3 = GetProcessCr3(process);
        ObDereferenceObject(process);

        RtlCopyMemory(sharedMem->Data, &cr3, sizeof(cr3));
        sharedMem->ResponseSize = sizeof(cr3);
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

void ProcessCommand(PSHARED_MEMORY sharedMem) {
    __try {
        if (!sharedMem) return;
        
        NTSTATUS status = STATUS_SUCCESS;

        switch (sharedMem->Command) {
        case CMD_READ_MEMORY:
            status = HandleReadMemory(sharedMem);
            break;
        case CMD_GET_PROCESS_BASE:
            status = HandleGetProcessBase(sharedMem);
            break;
        case CMD_GET_DTB:
            status = HandleGetDtb(sharedMem);
            break;
        case CMD_SET_PROTECTED_PID:
            status = HandleSetProtectedPid(sharedMem);
            break;
        case CMD_MOUSE_CLICK:
            status = STATUS_NOT_IMPLEMENTED;
            break;
        case CMD_NONE:
            // No command, just return
            return;
        default:
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        sharedMem->Status = NT_SUCCESS(status) ? CMD_STATUS_COMPLETED : CMD_STATUS_ERROR;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (sharedMem) {
            sharedMem->Status = CMD_STATUS_ERROR;
        }
    }
}

// ============================================================================
// Shared memory using MmMapViewInSystemSpace for kernel mapping
// ============================================================================

NTSTATUS CreateSharedMemory() {
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING sectionName;
    LARGE_INTEGER sectionSize;
    SECURITY_DESCRIPTOR sd;
    PVOID sectionObject = NULL;
    
    if (g_SharedMemory != NULL) {
        return STATUS_SUCCESS;
    }
    
    status = RtlCreateSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    if (!NT_SUCCESS(status)) return status;
    
    status = RtlSetDaclSecurityDescriptor(&sd, TRUE, NULL, FALSE);
    if (!NT_SUCCESS(status)) return status;

    RtlInitUnicodeString(&sectionName, L"\\BaseNamedObjects\\Global\\{1C3E4F91-8B2D-4A6E-9F7C-2E5A1D8B4F6C}");
    InitializeObjectAttributes(&objAttr, &sectionName, OBJ_CASE_INSENSITIVE, NULL, &sd);
    sectionSize.QuadPart = sizeof(SHARED_MEMORY);

    // Create the section
    status = ZwCreateSection(&g_SectionHandle, SECTION_ALL_ACCESS, &objAttr, &sectionSize, PAGE_READWRITE, SEC_COMMIT, NULL);
    if (!NT_SUCCESS(status)) {
        g_SectionHandle = NULL;
        return status;
    }
    
    // Get section object pointer from handle
    status = ObReferenceObjectByHandle(
        g_SectionHandle,
        SECTION_ALL_ACCESS,
        NULL,
        KernelMode,
        &sectionObject,
        NULL
    );
    
    if (!NT_SUCCESS(status)) {
        ZwClose(g_SectionHandle);
        g_SectionHandle = NULL;
        return status;
    }
    
    // Map into system space (kernel address space accessible from any context)
    SIZE_T viewSize = sizeof(SHARED_MEMORY);
    status = MmMapViewInSystemSpace(sectionObject, &g_SharedMemory, &viewSize);
    
    ObDereferenceObject(sectionObject);
    
    if (!NT_SUCCESS(status) || !g_SharedMemory) {
        ZwClose(g_SectionHandle);
        g_SectionHandle = NULL;
        g_SharedMemory = NULL;
        return status;
    }
    
    RtlZeroMemory(g_SharedMemory, sizeof(SHARED_MEMORY));
    return STATUS_SUCCESS;
}

// ============================================================================
// IRP handlers (with safe exception handling)
// ============================================================================

NTSTATUS IoControlHandler(PDEVICE_OBJECT deviceObj, PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObj);
    
    __try {
        if (!irp) return STATUS_INVALID_PARAMETER;
        
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
        if (!stack) {
            irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }
        
        ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

        if (code == IOCTL_WD_GET_STATISTICS) {
            // Initialize shared memory on first call
            if (!g_SharedMemory) {
                CreateSharedMemory();
            }
        }
        else if (code == IOCTL_WD_QUERY_INFO) {
            // Process command only if shared memory exists
            if (g_SharedMemory) {
                ProcessCommand((PSHARED_MEMORY)g_SharedMemory);
            }
        }

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (irp) {
            irp->IoStatus.Status = STATUS_ACCESS_VIOLATION;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
        }
        return STATUS_ACCESS_VIOLATION;
    }
}

NTSTATUS CreateCloseHandler(PDEVICE_OBJECT deviceObj, PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObj);
    
    __try {
        if (irp) {
            irp->IoStatus.Status = STATUS_SUCCESS;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
        }
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

NTSTATUS UnsupportedHandler(PDEVICE_OBJECT deviceObj, PIRP irp) {
    UNREFERENCED_PARAMETER(deviceObj);
    
    __try {
        if (irp) {
            irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
            irp->IoStatus.Information = 0;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
        }
        return STATUS_NOT_SUPPORTED;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_NOT_SUPPORTED;
    }
}

// ============================================================================
// Driver initialization (called from system thread - with exception handling)
// ============================================================================

NTSTATUS InitializeDriver(PDRIVER_OBJECT drvObj, PUNICODE_STRING regPath) {
    UNREFERENCED_PARAMETER(regPath);

    __try {
        if (!drvObj) {
            return STATUS_INVALID_PARAMETER;
        }
        
        NTSTATUS status;
        PDEVICE_OBJECT deviceObj = NULL;
        UNICODE_STRING deviceName, symLink;

        RtlInitUnicodeString(&deviceName, L"\\Device\\WdFilterDrv");
        RtlInitUnicodeString(&symLink, L"\\DosDevices\\WdFilterDrv");

        status = IoCreateDevice(drvObj, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObj);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = IoCreateSymbolicLink(&symLink, &deviceName);
        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(deviceObj);
            return status;
        }

        for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
            drvObj->MajorFunction[i] = &UnsupportedHandler;
        }

        drvObj->MajorFunction[IRP_MJ_CREATE] = &CreateCloseHandler;
        drvObj->MajorFunction[IRP_MJ_CLOSE] = &CreateCloseHandler;
        drvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &IoControlHandler;

        deviceObj->Flags |= DO_BUFFERED_IO;
        deviceObj->Flags &= ~DO_DEVICE_INITIALIZING;

        g_DeviceObject = deviceObj;
        
        // Pre-create shared memory
        CreateSharedMemory();
        
        // Register ObCallbacks for process protection
        RegisterProtectionCallbacks();
        
        // Mark driver as ready
        InterlockedExchange(&g_DriverReady, 1);
        
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
    }
}

// ============================================================================
// System thread for deferred initialization (SAFE version)
// ============================================================================

void InitThread(PVOID context) {
    UNREFERENCED_PARAMETER(context);
    
    __try {
        // Short delay - just enough for kdmapper to clean up
        LARGE_INTEGER delay;
        delay.QuadPart = -30000000LL; // 3 seconds
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
        
        // Now call IoCreateDriver from system thread context
        IoCreateDriver(NULL, InitializeDriver);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent failure - nothing we can do here
    }
    
    PsTerminateSystemThread(STATUS_SUCCESS);
}

// ============================================================================
// Entry point - MUST return fast for kdmapper! (Minimal code path)
// ============================================================================

extern "C" NTSTATUS DriverEntry(
    _In_ PVOID kdmapperParam1,
    _In_ PVOID kdmapperParam2
)
{
    UNREFERENCED_PARAMETER(kdmapperParam1);
    UNREFERENCED_PARAMETER(kdmapperParam2);
    
    __try {
        HANDLE threadHandle = NULL;
        
        // Create system thread for deferred IoCreateDriver
        NTSTATUS status = PsCreateSystemThread(
            &threadHandle,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            InitThread,
            NULL
        );
        
        if (NT_SUCCESS(status) && threadHandle) {
            ZwClose(threadHandle);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent failure
    }
    
    // Return immediately - kdmapper requires this!
    return STATUS_SUCCESS;
}
