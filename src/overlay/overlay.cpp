#include "overlay.h"
#include <d3d11.h>
#include <iostream>
#include "../OS-ImGui/imgui/imgui.h"
#include "../OS-ImGui/imgui/imgui_impl_dx11.h"
#include "../OS-ImGui/imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
        
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

Overlay::Overlay() : overlayWindow(nullptr), device(nullptr), context(nullptr), 
                     swapChain(nullptr), renderTargetView(nullptr), isRunning(false) {
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
    ZeroMemory(&windowClass, sizeof(windowClass));
}

Overlay::~Overlay() {
    Cleanup();
}

bool Overlay::Initialize() {
    if (!CreateOverlayWindow()) {
        std::cerr << "[-] Failed to create window" << std::endl;
        return false;
    }
    
    if (!InitializeDirectX()) {
        std::cerr << "[-] Failed to initialize graphics" << std::endl;
        return false;
    }
    
    if (!InitializeImGui()) {
        std::cerr << "[-] Failed to initialize UI" << std::endl;
        return false;
    }
    
    isRunning = true;
    std::cout << "[+] Initialized successfully" << std::endl;
    return true;
}

bool Overlay::CreateOverlayWindow() {
    // Create transparent overlay window
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.hIcon = nullptr;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = L"Windows.UI.Core.CoreWindow";
    windowClass.hIconSm = nullptr;
    
    if (!RegisterClassEx(&windowClass)) {
        return false;
    }
    
    overlayWindow = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        L"Windows.UI.Core.CoreWindow",
        L"",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr,
        windowClass.hInstance,
        nullptr
    );
    
    if (!overlayWindow) {
        return false;
    }
    
    // Make window transparent
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_ALPHA);
    
    // Extend frame into client area
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(overlayWindow, &margins);
    
    ShowWindow(overlayWindow, SW_SHOW);
    UpdateWindow(overlayWindow);
    
    return true;
}

bool Overlay::InitializeDirectX() {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = screenWidth;
    sd.BufferDesc.Height = screenHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = overlayWindow;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain,
        &device,
        &featureLevel,
        &context
    ))) {
        return false;
    }

    // Create render target
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (backBuffer) {
        device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
        backBuffer->Release();
    }
    
    return true;
}

bool Overlay::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Disable imgui.ini
    
    // Setup style
    ImGui::StyleColorsDark();
    
    if (!ImGui_ImplWin32_Init(overlayWindow)) {
        return false;
    }
    
    if (!ImGui_ImplDX11_Init(device, context)) {
        return false;
    }
    
    return true;
}

void Overlay::BeginFrame() {
    // Handle Windows messages
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        if (msg.message == WM_QUIT) {
            isRunning = false;
        }
    }
    
    // Start ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Overlay::EndFrame() {
    ImGui::EndFrame();
    ImGui::Render();
    
    // Clear render target
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    context->ClearRenderTargetView(renderTargetView, clearColor);
    
    // Render ImGui
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    // Present
    swapChain->Present(1, 0);
}

void Overlay::Cleanup() {
    if (device) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    
    if (renderTargetView) {
        renderTargetView->Release();
        renderTargetView = nullptr;
    }
    
    if (swapChain) {
        swapChain->Release();
        swapChain = nullptr;
    }
    
    if (context) {
        context->Release();
        context = nullptr;
    }
    
    if (device) {
        device->Release();
        device = nullptr;
    }
    
    if (overlayWindow) {
        DestroyWindow(overlayWindow);
        UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        overlayWindow = nullptr;
    }
    
    isRunning = false;
}
