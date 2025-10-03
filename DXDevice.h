#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "dxUtils.h"
#include <dxgi1_6.h>

class Device
{
public:
	static Device& GetInstance()
	{
		static Device instance;
		return instance;
	}


	IDXGIFactory4* GetDxgiFactory() { return DxgiFactory.Get(); }
	ID3D12Device* GetD3DDevice() { return D3DDevice.Get(); }

private:
	void Init();

	Device() {
		Init();
	};
	~Device() {};



	Microsoft::WRL::ComPtr<IDXGIFactory4> DxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> D3DDevice;
};