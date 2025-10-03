#pragma once

#include <d3d12.h>

#include <wrl.h>
#include <string>
#include <unordered_map>


#include "DXRootSignature.h"
#include <memory>

class Material
{
public:
	Material(const std::string InName = "DefaultName", Microsoft::WRL::ComPtr<ID3D12RootSignature> InRootSig = nullptr, D3D12_GRAPHICS_PIPELINE_STATE_DESC InPipStateDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC());
	void InitPSO();
	void Init();
	void Destroy() {
		m_pso.Reset();
	}
	

	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() { return m_rootSignature; }
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPSO() { return m_pso; }
	std::string GetName() { return m_name; }
	
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
	std::string m_name;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
	bool bInit = false;
};

class MaterialManager
{
	public:
	static MaterialManager& GetInstance()
	{
		static MaterialManager instance;
		return instance;
	}

	~MaterialManager();


	Material& GetOrCreateMaterial(const std::string& name);

	void AddMaterial(Material* InMaterial) {
		Materials[InMaterial->GetName()] = std::shared_ptr<Material>(InMaterial);
	}

	int InitAllMaterial();
	int DestroyAllMaterial();


private:

	std::unordered_map<std::string, std::shared_ptr<Material>> Materials;

};