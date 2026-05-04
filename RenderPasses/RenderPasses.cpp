#include "RenderPasses.h"
#include "DXRootSignature.h"
#include "DXShader.h"
#include "DXDevice.h"
#include "Common.h"


void DeferredLightPass::Init(ID3D12Device* device)
{
	// signature
    DXRootSignature rootSigBuilder;

    // Index 0: b0 Light CB (Root CBV)
    rootSigBuilder.AddRootConstantBufferView(0);


    // t0 GBuffer0
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // t1 GBuffer1
    rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // t2 GBuffer2
    rootSigBuilder.AddSRVDescriptorTable(2, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // t3 GBuffer3
    rootSigBuilder.AddSRVDescriptorTable(3, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // t4 Shadow Mask
    rootSigBuilder.AddSRVDescriptorTable(4, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t5 Irradiance Map
    rootSigBuilder.AddSRVDescriptorTable(5, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t6 Prefilter Map
    rootSigBuilder.AddSRVDescriptorTable(6, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t7 BRDF LUT
    rootSigBuilder.AddSRVDescriptorTable(7, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t8 depth
    rootSigBuilder.AddSRVDescriptorTable(8, 1, D3D12_SHADER_VISIBILITY_PIXEL);


    //sampler
    // point sampler
    D3D12_STATIC_SAMPLER_DESC samplerPointClamp = {};
    samplerPointClamp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerPointClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.MipLODBias = 0;
    samplerPointClamp.MaxAnisotropy = 0;
    samplerPointClamp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerPointClamp.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerPointClamp.MinLOD = 0.0f;
    samplerPointClamp.MaxLOD = D3D12_FLOAT32_MAX;
    samplerPointClamp.ShaderRegister = 0; // 銆愬叧閿€戝繀椤绘槸 s0 鍖归厤 shader
    samplerPointClamp.RegisterSpace = 0;
    samplerPointClamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    //PCF 
    D3D12_STATIC_SAMPLER_DESC samplerShadow = {};
    samplerShadow.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplerShadow.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.MipLODBias = 0;
    samplerShadow.MaxAnisotropy = 0;
    samplerShadow.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplerShadow.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerShadow.MinLOD = 0.0f;
    samplerShadow.MaxLOD = D3D12_FLOAT32_MAX;
    samplerShadow.ShaderRegister = 1; // 銆愬叧閿€戝繀椤绘槸 s1 鍖归厤 shader
    samplerShadow.RegisterSpace = 0;
    samplerShadow.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    //IBL Linear + Clamp
    D3D12_STATIC_SAMPLER_DESC samplerLinear = {};
    samplerLinear.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerLinear.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerLinear.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerLinear.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerLinear.MipLODBias = 0;
    samplerLinear.MaxAnisotropy = 0; // 銆愬叧閿€戝鏋?Filter 涓嶆槸 ANISOTROPIC锛岃繖閲屽繀椤讳负 0锛屽惁鍒?Build 浼氭姤閿欙紒
    samplerLinear.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerLinear.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerLinear.MinLOD = 0.0f;
    samplerLinear.MaxLOD = D3D12_FLOAT32_MAX;
    samplerLinear.ShaderRegister = 2; // 銆愬叧閿€戝繀椤绘槸 s2 鍖归厤 shader
    samplerLinear.RegisterSpace = 0;
    samplerLinear.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSigBuilder.AddStaticSampler(samplerPointClamp);
    rootSigBuilder.AddStaticSampler(samplerShadow);
    rootSigBuilder.AddStaticSampler(samplerLinear);
    m_RootSig = rootSigBuilder.Build(device);


	// pso
    auto VS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"DeferredLightVS", L"DeferredLightShader.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto PS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"DeferredLightPS", L"DeferredLightShader.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = m_RootSig.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(VS->GetBufferPointer()), VS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(PS->GetBufferPointer()), PS->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));


}

void ZPrePass::Init(ID3D12Device* device)
{
    auto VsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ZPrepassVS", L"ZPrepassShader.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto PsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ZPrepassPS", L"ZPrepassShader.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = m_RootSig.Get(); // 澶嶇敤鐜板湪鐨?RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(VsByte->GetBufferPointer()), VsByte->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(PsByte->GetBufferPointer()), PsByte->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 蹇呴』鍖归厤 InitDepthStencilBuffer 閲岀殑 dsvDesc
    psoDesc.NumRenderTargets = 0;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void ShadowPass::Init(ID3D12Device* device)
{
    auto ShadowVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowVS", L"ShadowMapping.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = m_RootSig.Get(); // 澶嶇敤鐜板湪鐨?RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(ShadowVS->GetBufferPointer()), ShadowVS->GetBufferSize() };
    psoDesc.PS = { nullptr, 0 };

    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    rasterizerDesc.DepthBias = 100;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 1.0f;
    psoDesc.RasterizerState = rasterizerDesc;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 蹇呴』鍖归厤 InitShadowMap 閲岀殑 dsvDesc
    psoDesc.NumRenderTargets = 0; // 鎴戜滑涓嶈緭棰滆壊
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void ShadowMaskPass::Init(ID3D12Device* device)
{
    auto ShadowMaskVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowMaskVS", L"ShadowMaskShader.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto ShadowMaskPS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowMaskPS", L"ShadowMaskShader.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = m_RootSig.Get(); // 澶嶇敤鐜板湪鐨?RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(ShadowMaskVS->GetBufferPointer()), ShadowMaskVS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(ShadowMaskPS->GetBufferPointer()), ShadowMaskPS->GetBufferSize() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //PsoDesc.DepthStencilState.DepthEnable = TRUE;
    // 1. 鑾峰彇榛樿璁剧疆
    CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);

    // 2. 鏀逛负 "灏忎簬绛変簬" (LESS_EQUAL)锛岃繖鏍风浉鍚屾繁搴︾殑鍍忕礌涔熻兘閫氳繃娴嬭瘯
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 3. 鍏抽棴娣卞害鍐欏叆 (MainPass 宸茬粡鍐欒繃娣卞害浜嗭紝杩欓噷鍙)
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.DepthStencilState = depthDesc;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));

}

void SkyboxPass::Init(ID3D12Device* device)
{
    auto SkyVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"SkyVS", L"Skybox.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto SkyPS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"SkyPS", L"Skybox.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = m_RootSig.Get(); // 澶嶇敤鐜板湪鐨?RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(SkyVS->GetBufferPointer()), SkyVS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(SkyPS->GetBufferPointer()), SkyPS->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void BasePass::Init(ID3D12Device* device)
{
    auto VS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"BasePassVS", L"BasePass.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto PS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"BasePassPS", L"BasePass.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = m_RootSig.Get(); // 澶嶇敤鐜板湪鐨?RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(VS->GetBufferPointer()), VS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(PS->GetBufferPointer()), PS->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState = depthDesc;

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.NumRenderTargets = 3;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));

}

void GI_VoxelBuildPass::Init(ID3D12Device* device)
{
    m_IsCompute = true;
    // signature
    DXRootSignature rootSigBuilder;

	rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL); // u0 Voxel Data RWTexture3D
    m_RootSig = rootSigBuilder.Build(device);

    auto CsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"GI_VoxelBuildCS", L"GI_VoxelBuild.hlsl", "CSMain", "cs_5_0"
    )->GetBytecode();
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSig.Get();
    psoDesc.CS = { reinterpret_cast<BYTE*>(CsByte->GetBufferPointer()), CsByte->GetBufferSize() };
    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Compute PSO");
    }
}

void GI_Cascade0Pass::Init(ID3D12Device* device)
{
    m_IsCompute = true;
    DXRootSignature rootSigBuilder;
	rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL); // t0 Voxel Data Texture3D
    rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL); // u0 Cascade0 RWStructuredBuffer
    m_RootSig = rootSigBuilder.Build(device);


    auto CsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"GI_Cascade0CS", L"GI_Cascade0.hlsl", "CSMain", "cs_5_0"
    )->GetBytecode();
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSig.Get();
    psoDesc.CS = { reinterpret_cast<BYTE*>(CsByte->GetBufferPointer()), CsByte->GetBufferSize() };
    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Compute PSO");
    }

}

void GI_ViewPass::Init(ID3D12Device* device)
{
    // m_IsCompute = true;

    DXRootSignature rootSigBuilder;
    // Index 0: b0 LightCB (复用 LightConstants 布局)
    rootSigBuilder.AddRootConstantBufferView(0);
    // Index 1: t0 voxelGrid (Texture3D)
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // Index 2: t1 cascade0 (StructuredBuffer)
    rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig = rootSigBuilder.Build(device);

    auto VS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"GI_ViewVS", L"GI_View.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto PS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"GI_ViewPS", L"GI_View.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = m_RootSig.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(VS->GetBufferPointer()), VS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(PS->GetBufferPointer()), PS->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void GI_CascadePass::Init(ID3D12Device* device)
{
    m_IsCompute = true;
    DXRootSignature rootSigBuilder;
	// Index 0: t0 voxelGrid(Texture3D) SRV t0
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
    // Index 1: upper cascade (StructuredBuffer) SRV t1
    rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_ALL);
	// Index 2: current cascade RWStructuredBuffer u0
    rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
	// Index 3: b0 cascade constants
    rootSigBuilder.Add32BitConstants(0, 10, 0); // b0, 10 个值, space 0
    // Index 4: preFrameCascade0 buffer SRV t2
    rootSigBuilder.AddSRVDescriptorTable(2, 1, D3D12_SHADER_VISIBILITY_ALL);

    m_RootSig = rootSigBuilder.Build(device);

    auto CsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"GI_CascadeCS", L"GI_Cascade.hlsl", "CSMain", "cs_5_0"
    )->GetBytecode();
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSig.Get();
    psoDesc.CS = { reinterpret_cast<BYTE*>(CsByte->GetBufferPointer()), CsByte->GetBufferSize() };
    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Compute PSO");
    }

}
