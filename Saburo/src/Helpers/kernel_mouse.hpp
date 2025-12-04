#pragma once
#include <Windows.h>
#include <cstdint>

// IOCTL code for mouse movement - must match driver definition
#define IOCTL_MOUSE_MOVE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Mouse movement data structure
struct MouseData {
    LONG dx;  // relative X movement
    LONG dy;  // relative Y movement
};

// NtUserInjectMouseInput syscall (undocumented)
typedef BOOL(WINAPI* pNtUserInjectMouseInput)(PMOUSEINPUT pMouseInput, UINT cInputs);

namespace KernelMouse {
    /**
     * Initialize the kernel driver connection for mouse input
     * @param deviceLink Wide-string device symbolic link (e.g., L"\\\\.\\HiroyoshiDriver")
     * @return true if successfully connected to driver
     */
    bool Initialize(const wchar_t* deviceLink);

    /**
     * Move mouse cursor using kernel-level input injection
     * @param dx Relative X movement (positive = right, negative = left)
     * @param dy Relative Y movement (positive = down, negative = up)
     * @return true if movement command was successfully sent to driver
     */
    bool MoveMouse(LONG dx, LONG dy);

    /**
     * Smoothly move mouse to target screen position over multiple frames
     * @param targetX Target X screen coordinate
     * @param targetY Target Y screen coordinate
     * @param smoothing Smoothing factor (1.0 = instant, higher = slower/smoother)
     * @return true if movement command was successful
     */
    bool SmoothMoveTo(int targetX, int targetY, float smoothing = 2.5f);

    /**
     * Get current mouse cursor position
     * @param outX Output X coordinate
     * @param outY Output Y coordinate
     * @return true if position was retrieved successfully
     */
    bool GetCursorPosition(int& outX, int& outY);

    /**
     * Cleanup driver handle
     */
    void Shutdown();

    /**
     * Check if driver is connected and ready
     * @return true if driver handle is valid
     */
    bool IsInitialized();
}
