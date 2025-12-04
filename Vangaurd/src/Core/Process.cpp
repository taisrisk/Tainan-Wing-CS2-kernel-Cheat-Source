#include "process.hpp"
#include <tlhelp32.h>
#include <string>

Process::Process(const char* processName) : hProcess(nullptr), pid(0), base(0) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(snapshot, &pe32)) {
            do {
                if (strcmp(pe32.szExeFile, processName) == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }
}

Process::~Process() {
    if (hProcess) CloseHandle(hProcess);
}

bool Process::Attach() {
    if (!pid) return false;
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;

    HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (moduleSnapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);
        if (Module32First(moduleSnapshot, &me32)) {
            do {
                if (strcmp(me32.szModule, "Overwatch.exe") == 0) {
                    base = (uintptr_t)me32.modBaseAddr;
                    break;
                }
            } while (Module32Next(moduleSnapshot, &me32));
        }
        CloseHandle(moduleSnapshot);
    }
    return base != 0;
}

bool Process::ReadMemory(uintptr_t address, void* buffer, size_t size) {
    return ReadProcessMemory(hProcess, (void*)address, buffer, size, nullptr);
}

bool Process::WriteMemory(uintptr_t address, void* buffer, size_t size) {
    return WriteProcessMemory(hProcess, (void*)address, buffer, size, nullptr);
}

uintptr_t Process::FindPattern(const char* pattern, const char* mask) {
    MEMORY_BASIC_INFORMATION mbi = { 0 };
    uintptr_t address = base;
    while (VirtualQueryEx(hProcess, (void*)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_READWRITE || mbi.Protect & PAGE_EXECUTE_READWRITE)) {
            char* buffer = new char[mbi.RegionSize];
            if (ReadMemory(address, buffer, mbi.RegionSize)) {
                for (size_t i = 0; i < mbi.RegionSize - strlen(mask); ++i) {
                    bool found = true;
                    for (size_t j = 0; j < strlen(mask); ++j) {
                        if (mask[j] != '?' && pattern[j] != buffer[i + j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) {
                        delete[] buffer;
                        return address + i;
                    }
                }
            }
            delete[] buffer;
        }
        address += mbi.RegionSize;
    }
    return 0;
}
