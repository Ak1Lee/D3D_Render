#include "DXMaterial.h"
#include "DXDevice.h"
#include "iostream"
#include "render.h"



//void Material::InitPSO()
//{
//    // 锟斤拷始锟斤拷锟斤拷锟斤拷状态锟斤拷锟斤拷 (PSO)
//    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&m_pso)));
//    
//}

//void Material::Init()
//{
//    MaterialManager& Manager = MaterialManager::GetInstance();
//
//	Manager.AddMaterial(this);
//
//    if (bInit) return;
//    InitPSO();
//
//    // cbv锟斤拷始锟斤拷
//    auto DstSize = DXRender::GetInstance().GetSrvUavDescriptorSize();
//	auto AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
//	auto CbSize = (sizeof(MaterialConstants) + 255) & ~255; // 256锟街节讹拷锟斤拷
//
//	CpuCbvHandle = AllocInfo.CpuHandle;
//	GpuCbvHandle = AllocInfo.GpuHandle;
//	DescriptorIndex = AllocInfo.Index;
//    // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷源
//	CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
//	CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(CbSize);
//    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(
//        &HeapProps,
//        D3D12_HEAP_FLAG_NONE,
//        &ResourceDesc,
//        D3D12_RESOURCE_STATE_GENERIC_READ,
//        nullptr,
//        IID_PPV_ARGS(&ConstantBuffer)
//    ));
//    // 映锟戒常锟斤拷锟斤拷锟斤拷锟斤拷
//    CD3DX12_RANGE readRange(0, 0); // 锟斤拷锟斤拷锟斤拷锟饺★拷锟斤拷锟斤拷源锟斤拷锟斤拷锟皆凤拷围为0
//    ThrowIfFailed(ConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCbvDataBegin)));
//    // 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷图 (CBV)
//    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
//    cbvDesc.BufferLocation = ConstantBuffer->GetGPUVirtualAddress();
//    cbvDesc.SizeInBytes = CbSize;
//	Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&cbvDesc, CpuCbvHandle);
//
//    ConstantData.Albedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
//    ConstantData.Roughness = 0.5f;
//    ConstantData.Metallic = 0.0f;
//    ConstantData.AO = 1.0f;
//    memcpy(pCbvDataBegin, &ConstantData, sizeof(MaterialConstants));
//
//	bInit = true;
//}

void Material::Destroy()
{
 //   if(ConstantBuffer)
 //   {
	//	ConstantBuffer->Unmap(0, nullptr);
 //       ConstantBuffer.Reset();
 //       
	//}
 //   pCbvDataBegin = nullptr;
}
//
//void Material::Bind(ID3D12GraphicsCommandList* CommandList)
//{
//    if (!pCbvDataBegin) return;
//    if(bDirty)
//    {
//        // 锟斤拷锟铰筹拷锟斤拷锟斤拷锟斤拷锟斤拷
//        memcpy(pCbvDataBegin, &ConstantData, sizeof(MaterialConstants));
//        bDirty = false;
//	}
//	CommandList->SetPipelineState(m_pso.Get());
//	CommandList->SetGraphicsRootSignature(m_rootSignature.Get());
//
//    // b2 
//    CommandList->SetGraphicsRootDescriptorTable(2, GpuCbvHandle);
//	//CommandList->SetGraphicsRootConstantBufferView(2, ConstantBuffer->GetGPUVirtualAddress());
//}


MaterialManager::~MaterialManager()
{
	// DestroyAllMaterial();
}

Material& MaterialManager::GetOrCreateMaterial(const std::string& name)
{

    if(Materials.find(name) == Materials.end())
    {
		Materials[name] = std::make_shared<Material>(name);
	}

	return *Materials[name].get();
}

Material* MaterialManager::GetMaterialByName(const std::string& name)
{
    if (Materials.find(name) != Materials.end()) {
        return Materials[name].get();
    }
    return nullptr;
}
int MaterialManager::InitAllMaterial()
{
    for(auto& [name, material] : Materials)
    {
        // 锟斤拷始锟斤拷每锟斤拷锟斤拷锟斤拷
		// material->Init();

	}
    return 0;
}

int MaterialManager::DestroyAllMaterial()
{

    for (auto& [name, material] : Materials)
    {
        material->Destroy();
    }
    return 0;

}

//Material::Material(const std::string InName, Microsoft::WRL::ComPtr<ID3D12RootSignature> InRootSig, D3D12_GRAPHICS_PIPELINE_STATE_DESC PipStateDesc)
//{
//    m_name = InName;
//    m_rootSignature = InRootSig;
//    PsoDesc = PipStateDesc;
//	bInit = false;
//
//    Init();
//}

Material::Material(const std::string InName)
{
	Name = InName;
}
