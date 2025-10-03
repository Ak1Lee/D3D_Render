#include "DXMaterial.h"
#include "DXDevice.h"
#include "iostream"



void Material::InitPSO()
{
    // 初始化管线状态对象 (PSO)
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&m_pso)));
    
}

void Material::Init()
{


    MaterialManager& Manager = MaterialManager::GetInstance();

	Manager.AddMaterial(this);

    if (bInit) return;
    InitPSO();
	bInit = true;
}


MaterialManager::~MaterialManager()
{
	DestroyAllMaterial();
}

Material& MaterialManager::GetOrCreateMaterial(const std::string& name)
{

    if(Materials.find(name) == Materials.end())
    {
		Materials[name] = std::make_shared<Material>(name);
	}

	return *Materials[name].get();
}

int MaterialManager::InitAllMaterial()
{
    for(auto& [name, material] : Materials)
    {
        // 初始化每个材质
		material->Init();

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

Material::Material(const std::string InName, Microsoft::WRL::ComPtr<ID3D12RootSignature> InRootSig, D3D12_GRAPHICS_PIPELINE_STATE_DESC PipStateDesc)
{
    m_name = InName;
    m_rootSignature = InRootSig;
    PsoDesc = PipStateDesc;
	bInit = false;

    Init();
}
