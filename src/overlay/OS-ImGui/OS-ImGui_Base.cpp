#include "OS-ImGui_Base.h"
//#include "..\Font\HarmonyOS_SansSC_Bold.h"

namespace OSImGui
{
    bool OSImGui_Base::InitImGui(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontDefault();
        (void)io;

        ImFontAtlas* fontAtlas = new ImFontAtlas();
        ImFontConfig arialConfig;
        arialConfig.FontDataOwnedByAtlas = false;
        ImFont* arialFont = fontAtlas->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 20.0f, &arialConfig, io.Fonts->GetGlyphRangesAll());

        ImFontConfig ESPConfig;
        ESPConfig.FontDataOwnedByAtlas = false;
        ImFont* ESPFont = fontAtlas->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 18.0f, &arialConfig, io.Fonts->GetGlyphRangesAll());
        
        // Big font for sidebar buttons (like overlayexp sidefont 30px)
        ImFontConfig sidebarConfig;
        sidebarConfig.FontDataOwnedByAtlas = false;
        ImFont* sidebarFont = fontAtlas->AddFontFromFileTTF("c:\\Windows\\Fonts\\arial.ttf", 30.0f, &sidebarConfig, io.Fonts->GetGlyphRangesDefault());
        
        io.Fonts = fontAtlas;

        // ImGui::StyleColorsEnemyMouse();
        ImGui::AimStarDefaultStyle();
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