#include "kernel_mouse.hpp"
#include <iostream>
#include <windows.h>

namespace KernelMouse {

    static HANDLE hDriver = INVALID_HANDLE_VALUE;
    static pNtUserInjectMouseInput NtUserInjectMouseInput = nullptr;

    bool Initialize(const wchar_t* deviceLink) {
        if (hDriver != INVALID_HANDLE_VALUE) {
            Shutdown();
        }

        hDriver = CreateFileW(
            deviceLink,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hDriver == INVALID_HANDLE_VALUE) {
            return false;
        }

        // Load NtUserInjectMouseInput from win32u.dll (silently)
        HMODULE win32u = LoadLibraryA("win32u.dll");
        if (win32u) {
            NtUserInjectMouseInput = (pNtUserInjectMouseInput)GetProcAddress(win32u, "NtUserInjectMouseInput");
        }

        return true;
    }

    bool MoveMouse(LONG dx, LONG dy) {
        if (hDriver == INVALID_HANDLE_VALUE) {
            std::cerr << "[-] Driver not initialized\n";
            return false;
        }

        MouseData data;
        data.dx = dx;
        data.dy = dy;

        DWORD bytesReturned = 0;
        BOOL result = DeviceIoControl(
            hDriver,
            IOCTL_MOUSE_MOVE,
            &data,
            sizeof(MouseData),
            nullptr,
            0,
            &bytesReturned,
            nullptr
        );

        if (!result) {
            DWORD error = GetLastError();
            std::cerr << "[-] Mouse movement IOCTL failed. Error: " << error << "\n";
            return false;
        }

        // After successful IOCTL, inject mouse input
        if (NtUserInjectMouseInput) {
            MOUSEINPUT mouseInput = { 0 };
            mouseInput.dx = dx;
            mouseInput.dy = dy;
            mouseInput.mouseData = 0;
            mouseInput.dwFlags = MOUSEEVENTF_MOVE;
            mouseInput.time = 0;
            mouseInput.dwExtraInfo = 0;

            if (!NtUserInjectMouseInput(&mouseInput, 1)) {
                std::cerr << "[-] NtUserInjectMouseInput failed\n";
                return false;
            }
        }
        else {
            // Fallback to SendInput if NtUserInjectMouseInput not available
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = dx;
            input.mi.dy = dy;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.time = 0;
            input.mi.dwExtraInfo = 0;

            if (SendInput(1, &input, sizeof(INPUT)) != 1) {
                std::cerr << "[-] SendInput fallback failed\n";
                return false;
            }
        }

        return true;
    }

    bool SmoothMoveTo(int targetX, int targetY, float smoothing) {
        int currentX, currentY;
        if (!GetCursorPosition(currentX, currentY)) {
            return false;
        }

        // Calculate delta to target
        LONG deltaX = targetX - currentX;
        LONG deltaY = targetY - currentY;

        // Apply smoothing (divide by smoothing factor for gradual movement)
        LONG moveX = static_cast<LONG>(deltaX / smoothing);
        LONG moveY = static_cast<LONG>(deltaY / smoothing);

        // Only move if there's significant delta (avoid micro-jitter)
        if (abs(moveX) < 1 && abs(moveY) < 1) {
            return true; // Already at target
        }

        return MoveMouse(moveX, moveY);
    }

    bool GetCursorPosition(int& outX, int& outY) {
        POINT cursorPos;
        if (!::GetCursorPos(&cursorPos)) {
            return false;
        }

        outX = static_cast<int>(cursorPos.x);
        outY = static_cast<int>(cursorPos.y);
        return true;
    }

    void Shutdown() {
        if (hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(hDriver);
            hDriver = INVALID_HANDLE_VALUE;
        }
    }

    bool IsInitialized() {
        return hDriver != INVALID_HANDLE_VALUE;
    }

} // namespace KernelMouse
