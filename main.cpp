#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tchar.h>
#include <wrl.h>

#include <dxgi1_6.h>
#include <DirectXMath.h>


//for d3d12
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "d3dx12.h"
#include <iostream>
#include <comdef.h>

#include "Timer.h"
#include "dxUtils.h"
#include "render.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindowClass";
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindow(
        L"MyWindowClass", L"My DX12 App",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    //AllocConsole(); // 申请控制台
    //freopen("CONOUT$", "w", stdout); // 绑定 std::cout 到控制台
    //freopen("CONOUT$", "w", stderr);

    std::cout << "Hello Console!" << std::endl;

    
    DXRender render;

    render.InitDX(hwnd);


    float AccumTime = 0.0f;

    MSG msg = {};
    while (GetMessage(&msg, nullptr,0,0))
    {
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        render.Draw();
        Sleep(100);

    }

    return (int)msg.wParam;


}

