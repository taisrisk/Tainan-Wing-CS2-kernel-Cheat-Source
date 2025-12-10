#include "OS-ImGui_Base.h"

namespace OSImGui
{
    // Keyboard hook for capturing input
    HHOOK g_keyboard_hook = nullptr;
    
    // Convert Windows VK codes to ImGui keys
    static ImGuiKey VKToImGuiKey(DWORD vk)
    {
        switch (vk)
        {
            case VK_TAB: return ImGuiKey_Tab;
            case VK_LEFT: return ImGuiKey_LeftArrow;
            case VK_RIGHT: return ImGuiKey_RightArrow;
            case VK_UP: return ImGuiKey_UpArrow;
            case VK_DOWN: return ImGuiKey_DownArrow;
            case VK_PRIOR: return ImGuiKey_PageUp;
            case VK_NEXT: return ImGuiKey_PageDown;
            case VK_HOME: return ImGuiKey_Home;
            case VK_END: return ImGuiKey_End;
            case VK_INSERT: return ImGuiKey_Insert;
            case VK_DELETE: return ImGuiKey_Delete;
            case VK_BACK: return ImGuiKey_Backspace;
            case VK_SPACE: return ImGuiKey_Space;
            case VK_RETURN: return ImGuiKey_Enter;
            case VK_ESCAPE: return ImGuiKey_Escape;
            case VK_OEM_7: return ImGuiKey_Apostrophe;
            case VK_OEM_COMMA: return ImGuiKey_Comma;
            case VK_OEM_MINUS: return ImGuiKey_Minus;
            case VK_OEM_PERIOD: return ImGuiKey_Period;
            case VK_OEM_2: return ImGuiKey_Slash;
            case VK_OEM_1: return ImGuiKey_Semicolon;
            case VK_OEM_PLUS: return ImGuiKey_Equal;
            case VK_OEM_4: return ImGuiKey_LeftBracket;
            case VK_OEM_5: return ImGuiKey_Backslash;
            case VK_OEM_6: return ImGuiKey_RightBracket;
            case VK_OEM_3: return ImGuiKey_GraveAccent;
            case VK_CAPITAL: return ImGuiKey_CapsLock;
            case VK_SCROLL: return ImGuiKey_ScrollLock;
            case VK_NUMLOCK: return ImGuiKey_NumLock;
            case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
            case VK_PAUSE: return ImGuiKey_Pause;
            case VK_NUMPAD0: return ImGuiKey_Keypad0;
            case VK_NUMPAD1: return ImGuiKey_Keypad1;
            case VK_NUMPAD2: return ImGuiKey_Keypad2;
            case VK_NUMPAD3: return ImGuiKey_Keypad3;
            case VK_NUMPAD4: return ImGuiKey_Keypad4;
            case VK_NUMPAD5: return ImGuiKey_Keypad5;
            case VK_NUMPAD6: return ImGuiKey_Keypad6;
            case VK_NUMPAD7: return ImGuiKey_Keypad7;
            case VK_NUMPAD8: return ImGuiKey_Keypad8;
            case VK_NUMPAD9: return ImGuiKey_Keypad9;
            case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
            case VK_DIVIDE: return ImGuiKey_KeypadDivide;
            case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
            case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
            case VK_ADD: return ImGuiKey_KeypadAdd;
            case VK_LSHIFT: return ImGuiKey_LeftShift;
            case VK_LCONTROL: return ImGuiKey_LeftCtrl;
            case VK_LMENU: return ImGuiKey_LeftAlt;
            case VK_LWIN: return ImGuiKey_LeftSuper;
            case VK_RSHIFT: return ImGuiKey_RightShift;
            case VK_RCONTROL: return ImGuiKey_RightCtrl;
            case VK_RMENU: return ImGuiKey_RightAlt;
            case VK_RWIN: return ImGuiKey_RightSuper;
            case VK_APPS: return ImGuiKey_Menu;
            case '0': return ImGuiKey_0;
            case '1': return ImGuiKey_1;
            case '2': return ImGuiKey_2;
            case '3': return ImGuiKey_3;
            case '4': return ImGuiKey_4;
            case '5': return ImGuiKey_5;
            case '6': return ImGuiKey_6;
            case '7': return ImGuiKey_7;
            case '8': return ImGuiKey_8;
            case '9': return ImGuiKey_9;
            case 'A': return ImGuiKey_A;
            case 'B': return ImGuiKey_B;
            case 'C': return ImGuiKey_C;
            case 'D': return ImGuiKey_D;
            case 'E': return ImGuiKey_E;
            case 'F': return ImGuiKey_F;
            case 'G': return ImGuiKey_G;
            case 'H': return ImGuiKey_H;
            case 'I': return ImGuiKey_I;
            case 'J': return ImGuiKey_J;
            case 'K': return ImGuiKey_K;
            case 'L': return ImGuiKey_L;
            case 'M': return ImGuiKey_M;
            case 'N': return ImGuiKey_N;
            case 'O': return ImGuiKey_O;
            case 'P': return ImGuiKey_P;
            case 'Q': return ImGuiKey_Q;
            case 'R': return ImGuiKey_R;
            case 'S': return ImGuiKey_S;
            case 'T': return ImGuiKey_T;
            case 'U': return ImGuiKey_U;
            case 'V': return ImGuiKey_V;
            case 'W': return ImGuiKey_W;
            case 'X': return ImGuiKey_X;
            case 'Y': return ImGuiKey_Y;
            case 'Z': return ImGuiKey_Z;
            case VK_F1: return ImGuiKey_F1;
            case VK_F2: return ImGuiKey_F2;
            case VK_F3: return ImGuiKey_F3;
            case VK_F4: return ImGuiKey_F4;
            case VK_F5: return ImGuiKey_F5;
            case VK_F6: return ImGuiKey_F6;
            case VK_F7: return ImGuiKey_F7;
            case VK_F8: return ImGuiKey_F8;
            case VK_F9: return ImGuiKey_F9;
            case VK_F10: return ImGuiKey_F10;
            case VK_F11: return ImGuiKey_F11;
            case VK_F12: return ImGuiKey_F12;
            default: return ImGuiKey_None;
        }
    }
    
    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
            ImGuiIO& io = ImGui::GetIO();
            
            // Convert virtual key to ImGui key
            ImGuiKey key = VKToImGuiKey(pKeyboard->vkCode);
            
            if (key != ImGuiKey_None)
            {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    io.AddKeyEvent(key, true);
                }
                else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                {
                    io.AddKeyEvent(key, false);
                }
            }
        }
        
        return CallNextHookEx(g_keyboard_hook, nCode, wParam, lParam);
    }
    
    bool OSImGui_Base::InitImGui(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontDefault();
        (void)io;

        //ImWchar ranges[] = {
        //    0x0020, 0x007F,   // Basic Latin characters
        //    0x00A0, 0x00FF,   // Latin characters with diacritics
        //    0x0400, 0x04FF,   // Cyrillic
        //    0x4E00, 0x9FFF,   // Simplified Chinese characters
        //    0x3040, 0x309F,   // Japanese Hiragana
        //    0x30A0, 0x30FF,   // Japanese Katakana
        //    0xFF00, 0xFFEF,   // Full-width characters
        //    0x2500, 0x257F,   // Lines and borders
        //    0x2600, 0x26FF,   // Weather symbols and other basic symbols
        //    0x2700, 0x27BF,   // Check symbols and other graphic symbols
        //    0x1F600, 0x1F64F, // Emoji
        //    0 // End of ranges
        //};


        ImFontAtlas* fontAtlas = new ImFontAtlas();
        ImFontConfig arialConfig;
        arialConfig.FontDataOwnedByAtlas = false;

        
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.OversampleH = 3;
        iconConfig.OversampleV = 3;
        iconConfig.FontDataOwnedByAtlas = false;


        io.Fonts = fontAtlas;

        ImGui::NovazBestingDefaultStyle();
        io.LogFilename = nullptr;

        if (!ImGui_ImplWin32_Init(Window.hWnd))
            throw OSException("ImGui_ImplWin32_Init() call failed.");
        if (!ImGui_ImplDX11_Init(device, device_context))
            throw OSException("ImGui_ImplDX11_Init() call failed.");

        return true;
    }

    void OSImGui_Base::CleanImGui()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        g_Device.CleanupDeviceD3D();
        DestroyWindow(Window.hWnd);
        UnregisterClassA(Window.ClassName.c_str(), Window.hInstance);
    }

    std::wstring OSImGui_Base::StringToWstring(std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
    }
}