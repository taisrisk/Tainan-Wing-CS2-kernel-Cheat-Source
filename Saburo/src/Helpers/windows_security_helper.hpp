#pragma once

#include <windows.h>
#include <wscapi.h>
#include <iwscapi.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "console_colors.hpp"

#pragma comment(lib, "wscapi.lib")
#pragma comment(lib, "ole32.lib")

namespace WindowsSecurityHelper {

    // Check if Windows Defender (or any AV via WSC) Real-Time Protection is enabled
    inline bool IsRealtimeProtectionEnabled() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool wasInitialized = SUCCEEDED(hr);

        IWSCProductList* pProductList = nullptr;
        hr = CoCreateInstance(
            __uuidof(WSCProductList),
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWSCProductList),
            reinterpret_cast<void**>(&pProductList)
        );

        if (FAILED(hr) || !pProductList) {
            if (wasInitialized) CoUninitialize();
            // Fallback: check via registry
            HKEY hKey;
            LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection",
                0, KEY_READ, &hKey);

            if (result == ERROR_SUCCESS) {
                DWORD disableRealtimeMonitoring = 0;
                DWORD dataSize = sizeof(DWORD);
                result = RegQueryValueExA(hKey, "DisableRealtimeMonitoring", nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(&disableRealtimeMonitoring), &dataSize);
                RegCloseKey(hKey);

                if (result == ERROR_SUCCESS) {
                    // 0 = RTP enabled, 1 = RTP disabled
                    return (disableRealtimeMonitoring == 0);
                }
            }

            // If we can't determine, assume it's enabled to be safe
            return true;
        }

        hr = pProductList->Initialize(WSC_SECURITY_PROVIDER_ANTIVIRUS);
        if (FAILED(hr)) {
            pProductList->Release();
            if (wasInitialized) CoUninitialize();
            return true; // Assume enabled if we can't check
        }

        LONG productCount = 0;
        hr = pProductList->get_Count(&productCount);
        if (FAILED(hr)) {
            pProductList->Release();
            if (wasInitialized) CoUninitialize();
            return true;
        }

        bool realtimeEnabled = false;
        for (LONG i = 0; i < productCount; i++) {
            IWscProduct* pProduct = nullptr;
            hr = pProductList->get_Item(i, &pProduct);
            if (SUCCEEDED(hr) && pProduct) {
                BSTR productName = nullptr;
                pProduct->get_ProductName(&productName);

                // Treat any AV product in ON state as RTP enabled
                WSC_SECURITY_PRODUCT_STATE productState;
                if (SUCCEEDED(pProduct->get_ProductState(&productState))) {
                    if (productState == WSC_SECURITY_PRODUCT_STATE_ON) {
                        realtimeEnabled = true;
                    }
                }
                if (productName) SysFreeString(productName);
                pProduct->Release();
            }
        }

        pProductList->Release();
        if (wasInitialized) CoUninitialize();

        return realtimeEnabled;
    }

    // Open Windows Security settings
    inline void OpenWindowsSecuritySettings() {
        ConsoleColor::PrintInfo("Opening Windows Security settings...");
        ShellExecuteA(nullptr, "open", "windowsdefender://threatsettings", nullptr, nullptr, SW_SHOW);
    }

    // Wait for user to close Windows Security settings window
    inline void WaitForSecurityWindowClose() {
        ConsoleColor::PrintInfo("Waiting for you to close Windows Security settings...");
        
        bool windowFound = true;
        while (windowFound) {
            windowFound = false;

            HWND hwnd = FindWindowA(nullptr, "Windows Security");
            if (hwnd == nullptr) {
                hwnd = FindWindowA(nullptr, "Virus & threat protection");
            }
            if (hwnd == nullptr) {
                hwnd = FindWindowA(nullptr, "Windows Defender");
            }

            if (hwnd != nullptr && IsWindowVisible(hwnd)) {
                windowFound = true;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        ConsoleColor::PrintSuccess("Windows Security settings closed");
    }

    // Guide user through disabling real-time protection (with post-close polling)
    inline bool GuideUserToDisableRealtimeProtection() {
        if (!IsRealtimeProtectionEnabled()) {
            ConsoleColor::PrintSuccess("Real-time protection is already disabled");
            return true;
        }

        std::vector<std::string> boxLines = {
            "",
            "The driver files will be blocked by",
            "Windows Defender Real-Time Protection.",
            "",
            "You MUST disable it to continue.",
            "",
            "STEP-BY-STEP INSTRUCTIONS:",
            "==============================",
            "",
            "1. Press ENTER to open Windows Security",
            "2. Click 'Virus & threat protection'",
            "3. Under 'Virus & threat protection",
            "   settings', click 'Manage settings'",
            "4. Turn OFF 'Real-time protection'",
            "5. Click 'Yes' on the UAC prompt",
            "6. Close the Windows Security window",
            "",
            "IMPORTANT:",
            "- If Windows asks to 'Send sample to",
            "  Microsoft', click 'Don't Send'",
            "- This happens randomly (50/50 chance)",
            ""
        };

        ConsoleColor::PrintBox("WINDOWS SECURITY WARNING", boxLines);
        
        ConsoleColor::SetColor(ConsoleColor::YELLOW);
        std::cout << "Press ENTER to open Windows Security settings...";
        ConsoleColor::Reset();
        
        std::cin.get();

        OpenWindowsSecuritySettings();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        WaitForSecurityWindowClose();
        // Poll for status for up to ~10 seconds to allow Defender to apply the change
        ConsoleColor::PrintInfo("Verifying Real-Time Protection status...");
        {
            const int retries = 20; // 20 * 500ms = 10s
            for (int i = 0; i < retries; ++i) {
                if (!IsRealtimeProtectionEnabled()) {
                    ConsoleColor::PrintSuccess("Real-Time Protection successfully disabled!");
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        {
            ConsoleColor::PrintError("Real-Time Protection is still enabled");
            ConsoleColor::PrintWarning("Please make sure you followed all the steps");
            
            ConsoleColor::SetColor(ConsoleColor::YELLOW);
            std::cout << "\nDo you want to try again? (y/n): ";
            ConsoleColor::Reset();
            
            char response;
            std::cin >> response;
            std::cin.ignore();

            if (response == 'y' || response == 'Y') {
                return GuideUserToDisableRealtimeProtection();
            }
            
            return false;
        }
    }
}
