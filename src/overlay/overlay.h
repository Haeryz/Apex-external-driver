#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>

// Forward declarations
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

class Overlay {
private:
    HWND overlayWindow;
    WNDCLASSEX windowClass;
    
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    
    int screenWidth;
    int screenHeight;
    bool isRunning;
    
public:
    Overlay();
    ~Overlay();
    
    bool Initialize();
    void Cleanup();
    
    // Render frame
    void BeginFrame();
    void EndFrame();
    
    // Getters
    HWND GetWindow() const { return overlayWindow; }
    ID3D11Device* GetDevice() const { return device; }
    ID3D11DeviceContext* GetContext() const { return context; }
    bool IsRunning() const { return isRunning; }
    int GetWidth() const { return screenWidth; }
    int GetHeight() const { return screenHeight; }
    
private:
    bool CreateOverlayWindow();
    bool InitializeDirectX();
    bool InitializeImGui();
};
