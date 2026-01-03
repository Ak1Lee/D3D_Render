#include "Texture.h"
#include "render.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
Texture::Texture()
{
}

Texture::Texture(const std::string InName)
{
	Name = InName;
}

Texture::~Texture()
{
	Resource.Reset();
	UploadHeap.Reset();
	Release();
}

void Texture::TransitionTo(ID3D12GraphicsCommandList* CmdList, D3D12_RESOURCE_STATES TargetState)
{
	if (CurrentState != TargetState)
	{
		CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			Resource.Get(),
			CurrentState,
			TargetState
		);
		CmdList->ResourceBarrier(1, &Barrier);
		CurrentState = TargetState;
	}
}

void Texture::SetAsRenderTarget(ID3D12GraphicsCommandList* CmdList, Texture* DepthTexture)
{
	D3D12_CPU_DESCRIPTOR_HANDLE DepthDSVHandle;

	TransitionTo(CmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRTV();
	if(DepthTexture && HasFlag(DepthTexture->ViewFlags, TextureViewFlags::DSV))
	{
		DepthTexture->TransitionTo(CmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		DepthDSVHandle = DepthTexture->GetDSV();
		CmdList->OMSetRenderTargets(
			1,
			&rtv,
			FALSE,
			&DepthDSVHandle
		);
	}
	else
	{
		CmdList->OMSetRenderTargets(
			1,
			&rtv,
			FALSE,
			nullptr
		);
	}
}

void Texture::BindSRV(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex)
{
	// 1. ×Ô¶¯ÅÐ¶Ï²¢ÇÐ»»×´Ì¬
	TransitionTo(CmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// 2. °ó¶¨¾ä±ú
	CmdList->SetGraphicsRootDescriptorTable(RootParamIndex, SRVHandle.GpuHandle);
}

void Texture::BindUAV(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex)
{
}

void Texture::LoadFromFile(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* CommandList,
	const std::string& Filename,
	bool IsSRGB, 
	bool IsHDR)
{
	int NrComponent;
	int PixelByteSize = 0;
	void* DataPtr = nullptr;
	if (IsHDR)
	{
		DataPtr = stbi_loadf(
			Filename.c_str(),
			&Width,
			&Height,
			&NrComponent,
			4
		);

		PixelByteSize = 16;
		Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	}


	else
	{
		DataPtr = stbi_load(
			Filename.c_str(),
			&Width,
			&Height,
			&NrComponent,
			4
		);
		PixelByteSize = 4; // 4 char 1 byte each = 4 byte per pixel
		Format = IsSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	}


	if(!DataPtr)
	{
		OutputDebugStringA("ERROR: Failed to load HDR texture: ");
		OutputDebugStringA(Filename.c_str());
		OutputDebugStringA("\n");
		return;
	}
	CreateResourceHeap(device, CommandList, DataPtr, PixelByteSize);
	CreateSRV(device, CommandList);

	stbi_image_free(DataPtr);

}

void Texture::CreateSRV(ID3D12Device* Device,
	ID3D12GraphicsCommandList* CmdList,
	D3D12_RESOURCE_DIMENSION dimension,
	bool isCubeMap)
{
	// texture_on_gpu -> Create SRView ->  descriptor to srv heap table
	auto size = DXRender::GetInstance().GetSrvUavDescriptorSize();
	SRVHandle = DXRender::GetInstance().AllocateDescriptorHandle(size);

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Format = Format;
	if (isCubeMap)
	{
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SrvDesc.TextureCube.MostDetailedMip = 0;
		SrvDesc.TextureCube.MipLevels = TexDesc.MipLevels;
		SrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	}
	else
	{
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Texture2D.MipLevels = 1;


	}
	Device->CreateShaderResourceView(Resource.Get(), &SrvDesc, SRVHandle.CpuHandle);


}

void Texture::CreateSRV(ID3D12Device* Device)
{
	m_SRVHandle = DXRender::GetInstance().AllocateDescriptorHandle(DXRender::GetInstance().GetSrvUavDescriptorSize());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if(HasFlag(ViewFlags, TextureViewFlags::DSV))
	{
		srvDesc.Format = GetSRVFormat(Format);
	}
	else
	{
		srvDesc.Format = Format;
	}

	//todo:cubemap:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = TexDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
	}
	Device->CreateShaderResourceView(Resource.Get(), &srvDesc, m_SRVHandle->CpuHandle);


}

void Texture::CreateUAV(ID3D12Device* Device)
{
	m_UAVHandle = DXRender::GetInstance().AllocateDescriptorHandle(DXRender::GetInstance().GetSrvUavDescriptorSize());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = Format;

	if (/*isCubeMap*/ false)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = 6;
	}
	else if (ArraySize > 1)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = ArraySize;
	}
	else
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
	}
	Device->CreateUnorderedAccessView(Resource.Get(), nullptr, &uavDesc, m_UAVHandle->CpuHandle);
}

void Texture::CreateRTV(ID3D12Device* Device)
{
	m_RTVHandle = DXRender::GetInstance().AllocateDescriptorHandle(DXRender::GetInstance().GetRtvDescriptorSize());
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	Device->CreateRenderTargetView(Resource.Get(), &rtvDesc, m_RTVHandle->CpuHandle);
}

void Texture::CreateDSV(ID3D12Device* Device)
{
	m_DSVHandle = DXRender::GetInstance().AllocateDescriptorHandle(DXRender::GetInstance().GetDsvDescriptorSize());

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = GetDSVFormat(Format);
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	Device->CreateDepthStencilView(Resource.Get(), &dsvDesc, m_DSVHandle->CpuHandle);
}

void Texture::CreateResourceHeap(ID3D12Device* device, ID3D12GraphicsCommandList* CmdList, const void* InData,int PixelByteSize)
{
	
	TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TexDesc.Width = Width;
	TexDesc.Height = Height;
	TexDesc.DepthOrArraySize = 1;
	TexDesc.MipLevels = 1;
	TexDesc.Format = Format;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	TexDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&TexDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&Resource));


	const UINT64 UploadBufferSize = GetRequiredIntermediateSize(Resource.Get(), 0, 1);
	CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC UploadDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);

	device->CreateCommittedResource(
		&UploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&UploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&UploadHeap));

	UploadHeap->SetName(L"Texture Upload Heap");
	D3D12_SUBRESOURCE_DATA TextureData = {};
	TextureData.pData = InData;
	TextureData.RowPitch = Width * PixelByteSize;
	TextureData.SlicePitch = TextureData.RowPitch * Height;
	UpdateSubresources(CmdList, Resource.Get(), UploadHeap.Get(), 0, 0, 1, &TextureData);
	CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	CmdList->ResourceBarrier(1, &Barrier);

}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetSRV()
{
	if(!m_SRVHandle.has_value())
	{
		if (!HasFlag(ViewFlags, TextureViewFlags::SRV))
		{
			throw std::runtime_error("Texture '" + Name + "' was not created with SRV flag!");
		}
		CreateSRV(Device::GetInstance().GetD3DDevice());
	}
	return m_SRVHandle->GpuHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetUAV()
{
	if(!m_UAVHandle.has_value())
	{
		if (!HasFlag(ViewFlags, TextureViewFlags::UAV))
		{
			throw std::runtime_error("Texture '" + Name + "' was not created with UAV flag!");
		}
		CreateUAV(Device::GetInstance().GetD3DDevice());
	}
	return m_UAVHandle->GpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE  Texture::GetRTV()
{
	if (!m_RTVHandle.has_value()) {
		if (!HasFlag(ViewFlags, TextureViewFlags::RTV)) {
			throw std::runtime_error("Texture '" + Name + "' was not created with RTV flag!");
		}
		CreateRTV(Device::GetInstance().GetD3DDevice());
	}
	return m_RTVHandle->CpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDSV()
{
	// todo 
	return D3D12_CPU_DESCRIPTOR_HANDLE();
}

DXGI_FORMAT Texture::GetTypelessFormat(DXGI_FORMAT format)
{
	switch (format) {
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	default:
		return format;
	}
}

DXGI_FORMAT Texture::GetDSVFormat(DXGI_FORMAT format)
{
	switch (format) {
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
	default:
		return format;
	}
}

DXGI_FORMAT Texture::GetSRVFormat(DXGI_FORMAT format)
{
	switch (format) {
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_R16_UNORM;
	default:
		return format;
	}
}

void Texture::Clear(ID3D12GraphicsCommandList* CmdList, const float* color)
{
	if(!HasFlag(ViewFlags, TextureViewFlags::RTV))
	{
		throw std::runtime_error("Texture '" + Name + "' was not created with RTV flag!");
	}
	TransitionTo(CmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	CmdList->ClearRenderTargetView(GetRTV(), color ? color : m_ClearColor, 0, nullptr);
}

void Texture::ClearDepth(ID3D12GraphicsCommandList* CmdList, float depth, UINT8 stencil)
{
	if (!HasFlag(ViewFlags, TextureViewFlags::DSV)) {
		throw std::runtime_error("Cannot clear depth on non-depth texture!");
	}

	TransitionTo(CmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	CmdList->ClearDepthStencilView(GetDSV(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		depth, stencil, 0, nullptr);

}
