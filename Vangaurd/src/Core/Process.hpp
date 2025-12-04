#pragma once
#include <windows.h>

class Process {
public:
    HANDLE hProcess;
    DWORD pid;
    uintptr_t base;

    Process(const char* processName);
    ~Process();
    bool Attach();
    bool ReadMemory(uintptr_t address, void* buffer, size_t size);
    bool WriteMemory(uintptr_t address, void* buffer, size_t size);
    uintptr_t FindPattern(const char* pattern, const char* mask);
};
