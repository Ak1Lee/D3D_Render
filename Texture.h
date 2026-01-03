#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>

#include "Common.h"
#include <optional>

using Microsoft::WRL::ComPtr;

enum class TextureViewFlags : uint32_t {
	None = 0,
	SRV = 1 << 0,  // Shader Resource View
	UAV = 1 << 1,  // Unordered Access View
	RTV = 1 << 2,  // Render Target View
	DSV = 1 << 3,  // Depth Stencil View
};

inline TextureViewFlags operator|(TextureViewFlags a, TextureViewFlags b) {
	return static_cast<TextureViewFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool HasFlag(TextureViewFlags flags, TextureViewFlags check) {
	return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(check)) != 0;
}

struct TextureDesc
{
	UINT Width = 1;
	UINT Height = 1;
	UINT Depth = 1;
	UINT MipLevels = 1;
	UINT ArraySize = 1;  
	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_DIMENSION Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	TextureViewFlags ViewFlags = TextureViewFlags::SRV;

	D3D12_CLEAR_VALUE* ClearValue = nullptr;

	static TextureDesc Create2D(UINT InWidth, UINT InHeight, DXGI_FORMAT InFormat, TextureViewFlags Views = TextureViewFlags::SRV)
	{
		TextureDesc Desc;
		Desc.Width = InWidth;
		Desc.Height = InHeight;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Format = InFormat;
		Desc.ViewFlags = Views;
		return Desc;
	}

	static TextureDesc CreateDepth(UINT InWidth, UINT InHeight,
		bool IsNeedSRV = true)
	{
		TextureDesc Desc;
		Desc.Width = InWidth;
		Desc.Height = InHeight;
		Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		Desc.ViewFlags = TextureViewFlags::DSV;
		if(IsNeedSRV)
		{
			Desc.ViewFlags = Desc.ViewFlags | TextureViewFlags::SRV;
		}
		return Desc;

	}

	static TextureDesc CreateCube(UINT InSize, DXGI_FORMAT InFormat, TextureViewFlags Views = TextureViewFlags::SRV)
	{
		TextureDesc Desc;
		Desc.Width = InSize;
		Desc.Height = InSize;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Format = InFormat;
		Desc.ViewFlags = Views;
		Desc.ArraySize = 6; // Cube map has 6 faces
		return Desc;
	}

};


//基本的srv 资源
class Texture
{
public:
	Texture();
	Texture(const std::string InName);
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	virtual ~Texture();

	void TransitionTo(ID3D12GraphicsCommandList* CmdList, D3D12_RESOURCE_STATES TargetState);


	void LoadFromFile(ID3D12Device* device,
		ID3D12GraphicsCommandList* CommandList,
		const std::string& Filename,
		bool IsSRGB = false, bool IsHDR = false);

	// 手动创建
	void Create(
		ID3D12GraphicsCommandList* CmdList,
		const TextureDesc& Desc,
		const void* InitialData = nullptr
	);

	void Release() {};



	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return SRVHandle.GpuHandle; }


	// view get
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRV();
	D3D12_GPU_DESCRIPTOR_HANDLE GetUAV();
	D3D12_CPU_DESCRIPTOR_HANDLE  GetRTV();
	D3D12_CPU_DESCRIPTOR_HANDLE  GetDSV();

	DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetDSVFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetSRVFormat(DXGI_FORMAT format);



	// clear
	void Clear(ID3D12GraphicsCommandList* CmdList, const float* color = nullptr);
	void ClearDepth(ID3D12GraphicsCommandList* CmdList, float depth = 1.0f, UINT8 stencil = 0);

	//set as rt, pass in a depth texture if needed
	void SetAsRenderTarget(ID3D12GraphicsCommandList* CmdList, Texture* DepthTexture);

	void BindSRV(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex);
	void BindUAV(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex);

	// Getter
	ID3D12Resource* GetResource() const { return Resource.Get(); }
	const std::string& GetName() const { return Name; }
	UINT GetWidth() const { return Width; }
	UINT GetHeight() const { return Height; }
	UINT GetArraySize() const { return ArraySize; }


protected:
	void CreateSRV(ID3D12Device* Device,
		ID3D12GraphicsCommandList* CmdList,
		D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		bool isCubeMap = false
	);

	void CreateSRV(ID3D12Device* Device);
	void CreateUAV(ID3D12Device* Device);
	void CreateRTV(ID3D12Device* Device);
	void CreateDSV(ID3D12Device* Device);


	void CreateResourceHeap(ID3D12Device* device, ID3D12GraphicsCommandList* CmdList, const void* InData, int PixelByteSize);

protected:
	ComPtr<ID3D12Resource> Resource;
	ComPtr<ID3D12Resource> UploadHeap;
	D3D12_RESOURCE_DESC TexDesc = {};
	std::string Name;

	DescriptorAllocation SRVHandle;

	// Views（使用 optional 实现懒加载）
	std::optional<DescriptorAllocation> m_SRVHandle;
	std::optional<DescriptorAllocation> m_UAVHandle;
	std::optional<DescriptorAllocation> m_RTVHandle;
	std::optional<DescriptorAllocation> m_DSVHandle;


	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureViewFlags ViewFlags = TextureViewFlags::None;
	D3D12_RESOURCE_STATES CurrentState = D3D12_RESOURCE_STATE_COMMON;

	float m_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float m_ClearDepth = 1.0f;
	UINT8 m_ClearStencil = 0;

	int Width = 0;
	int Height = 0;
	int MipLevels = 1;
	UINT ArraySize = 1;

	ID3D12Device* m_Device;

	bool bUseUAVTextureCube = false;
};
