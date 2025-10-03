#include "DXDevice.h"
#include <iostream>
void Device::Init()
{
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DxgiFactory)));

    // ¥¥Ω®device
    HRESULT HardwareCreateResult = D3D12CreateDevice(
        nullptr,             // ƒ¨»œ  ≈‰∆˜
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&D3DDevice)
    );

    if (HardwareCreateResult == E_NOINTERFACE)
    {
        std::cout << "Direct3D 12 is not supported on this device" << std::endl;

        std::cout << " Create Software Simulation" << std::endl;
        Microsoft::WRL::ComPtr<IDXGIAdapter> WARPAdapter;
        ThrowIfFailed(DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WARPAdapter)));
    }
    else
    {
        ThrowIfFailed(HardwareCreateResult);
        std::cout << " D3D12 Create Device Success!" << std::endl;
    }
}