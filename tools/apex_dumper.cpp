// Apex Legends Memory Dumper
// Dumps the decrypted r5apex.exe from memory for offset analysis
// Usage: Run this while Apex Legends is running, then upload r5apex.bin to:
// https://casualhacks.net/apexdream/apexdumper.html

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "psapi.lib")

DWORD GetProcessIdByName(const wchar_t* processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                CloseHandle(snapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return 0;
}

// Use PSAPI instead of Toolhelp for module enumeration (works better with protected processes)
bool GetModuleInfo(HANDLE hProcess, const wchar_t* moduleName, uintptr_t& baseAddress, SIZE_T& moduleSize) {
    HMODULE hMods[1024];
    DWORD cbNeeded;

    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            wchar_t szModName[MAX_PATH];
            if (GetModuleBaseNameW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
                if (_wcsicmp(szModName, moduleName) == 0) {
                    MODULEINFO modInfo;
                    if (GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo))) {
                        baseAddress = (uintptr_t)modInfo.lpBaseOfDll;
                        moduleSize = modInfo.SizeOfImage;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Alternative: Query memory regions directly
bool GetModuleInfoFromMemory(HANDLE hProcess, uintptr_t& baseAddress, SIZE_T& moduleSize) {
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = 0;
    uintptr_t firstBase = 0;
    SIZE_T totalSize = 0;
    
    // Find the first executable region (usually the main module)
    while (VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi))) {
        if (mbi.Type == MEM_IMAGE && mbi.State == MEM_COMMIT) {
            if (firstBase == 0) {
                firstBase = (uintptr_t)mbi.AllocationBase;
            }
            if ((uintptr_t)mbi.AllocationBase == firstBase) {
                totalSize = ((uintptr_t)mbi.BaseAddress + mbi.RegionSize) - firstBase;
            }
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (addr == 0) break; // Overflow protection
    }
    
    if (firstBase != 0 && totalSize > 0) {
        baseAddress = firstBase;
        moduleSize = totalSize;
        return true;
    }
    return false;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Apex Legends Memory Dumper\n";
    std::cout << "========================================\n\n";

    // Try both DX11 and DX12 versions
    const wchar_t* processNames[] = { L"r5apex_dx12.exe", L"r5apex.exe" };
    DWORD pid = 0;
    const wchar_t* foundProcess = nullptr;

    for (const auto& name : processNames) {
        pid = GetProcessIdByName(name);
        if (pid != 0) {
            foundProcess = name;
            break;
        }
    }

    if (pid == 0) {
        std::cout << "[!] Apex Legends not found!\n";
        std::cout << "[*] Please start Apex Legends first, then run this tool.\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::wcout << L"[+] Found " << foundProcess << L" (PID: " << pid << L")\n";

    // Open process with required permissions
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION,
        FALSE, pid
    );
    
    if (hProcess == NULL) {
        std::cout << "[!] Failed to open process! Error: " << GetLastError() << "\n";
        std::cout << "[*] Make sure you're running as Administrator.\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Process opened successfully!\n";

    // Get module info
    uintptr_t baseAddress = 0;
    SIZE_T moduleSize = 0;

    // Try PSAPI first
    std::cout << "[*] Trying PSAPI method...\n";
    if (!GetModuleInfo(hProcess, foundProcess, baseAddress, moduleSize)) {
        std::cout << "[*] PSAPI failed, trying memory query method...\n";
        if (!GetModuleInfoFromMemory(hProcess, baseAddress, moduleSize)) {
            std::cout << "[!] Failed to get module information!\n";
            CloseHandle(hProcess);
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }
    }

    std::cout << "[+] Base Address: 0x" << std::hex << baseAddress << std::dec << "\n";
    std::cout << "[+] Module Size: " << (moduleSize / 1024 / 1024) << " MB (" << moduleSize << " bytes)\n";

    // Sanity check - Apex is usually around 60-80MB
    if (moduleSize < 1024 * 1024 || moduleSize > 500 * 1024 * 1024) {
        std::cout << "[!] Warning: Module size seems unusual!\n";
    }

    std::cout << "[*] Reading memory... This may take a moment.\n";

    // Allocate buffer
    std::vector<uint8_t> buffer(moduleSize, 0);
    SIZE_T totalRead = 0;

    // Read in chunks to handle any unreadable pages
    const SIZE_T chunkSize = 0x1000; // 4KB pages
    int progress = 0;
    
    for (SIZE_T offset = 0; offset < moduleSize; offset += chunkSize) {
        SIZE_T toRead = min(chunkSize, moduleSize - offset);
        SIZE_T bytesRead = 0;
        
        ReadProcessMemory(hProcess, (LPCVOID)(baseAddress + offset), 
                          buffer.data() + offset, toRead, &bytesRead);
        totalRead += bytesRead;
        
        // Progress indicator (every 10%)
        int newProgress = (int)((offset * 100) / moduleSize);
        if (newProgress >= progress + 10) {
            progress = newProgress;
            std::cout << progress << "%...";
        }
    }

    CloseHandle(hProcess);
    std::cout << "100%\n";

    std::cout << "[+] Read " << (totalRead / 1024 / 1024) << " MB from memory\n";

    // Verify we got something useful (check for MZ header)
    if (buffer.size() >= 2 && buffer[0] == 'M' && buffer[1] == 'Z') {
        std::cout << "[+] Valid PE header detected!\n";
    } else {
        std::cout << "[!] Warning: No valid PE header found at base. This might still work.\n";
    }

    // Save to file
    std::string outputFile = "r5apex.bin";
    std::ofstream file(outputFile, std::ios::binary);
    if (!file) {
        std::cout << "[!] Failed to create output file!\n";
        std::cout << "[*] Make sure you have write permissions in the current directory.\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    file.write(reinterpret_cast<char*>(buffer.data()), moduleSize);
    file.close();

    // Get current directory for full path
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);

    std::cout << "\n========================================\n";
    std::cout << "[+] SUCCESS! Dumped to:\n";
    std::cout << "    " << currentDir << "\\" << outputFile << "\n";
    std::cout << "========================================\n\n";
    std::cout << "Next steps:\n";
    std::cout << "1. Go to: https://casualhacks.net/apexdream/apexdumper.html\n";
    std::cout << "2. Upload the r5apex.bin file\n";
    std::cout << "3. Wait for analysis (can take a minute)\n";
    std::cout << "4. Copy the offsets to src/memory/offsets.h\n";
    std::cout << "5. Rebuild ext.exe\n\n";

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
