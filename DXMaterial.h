#pragma once

#include <d3d12.h>

#include <wrl.h>
#include <string>
#include <unordered_map>


#include "DXRootSignature.h"
#include <memory>
#include "Common.h"
#include "Texture.h"

class Material
{
public:
	// Material(const std::string InName = "DefaultName", Microsoft::WRL::ComPtr<ID3D12RootSignature> InRootSig = nullptr, D3D12_GRAPHICS_PIPELINE_STATE_DESC InPipStateDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC());
	Material(const std::string InName = "DefaultMaterial");
	//void InitPSO();
	//void Init();
	void Destroy();
	void SetConstantData(const MaterialConstants& InConstantData) {
		ConstantData = InConstantData;
	}
	//void Bind(ID3D12GraphicsCommandList* CommandList);
	

	//Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() { return m_rootSignature; }
	//Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPSO() { return m_pso; }
	std::string GetName() { return Name; }

	void SetAlbedoTexture(std::shared_ptr<Texture> InTexture) { AlbedoTexture = InTexture; }
	void SetNormalTexture(std::shared_ptr<Texture> InTexture) { NormalTexture = InTexture; }
	void SetMetallicTexture(std::shared_ptr<Texture> InTexture) { MetallicTexture = InTexture; }

	bool HasAlbedoTexture() const { return AlbedoTexture != nullptr; }
	bool HasNormalTexture() const { return NormalTexture != nullptr; }
	bool HasMetallicTexture() const { return MetallicTexture != nullptr; }

	const MaterialConstants& GetConstantData() const { return ConstantData; }
	std::shared_ptr<Texture> GetAlbedoTexture() const { return AlbedoTexture; }
	std::shared_ptr<Texture> GetNormalTexture() const { return NormalTexture; }
	std::shared_ptr<Texture> GetMetallicTexture() const { return MetallicTexture; }

	
private:

	std::string Name;

	MaterialConstants ConstantData;

	//texture
	std::shared_ptr<Texture> AlbedoTexture;
	std::shared_ptr<Texture> NormalTexture;
	std::shared_ptr<Texture> MetallicTexture;



	
	// old method
	//Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	//Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
	//D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc;
	//bool bInit = false;
	//bool bDirty = true;
	//Microsoft::WRL::ComPtr<ID3D12Resource> ConstantBuffer;
	//UINT8* pCbvDataBegin = nullptr;

	//D3D12_CPU_DESCRIPTOR_HANDLE CpuCbvHandle; // 锟斤拷锟斤拷 View 锟斤拷
	//D3D12_GPU_DESCRIPTOR_HANDLE GpuCbvHandle; // Bind 锟斤拷
	//unsigned int DescriptorIndex = 0;
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
	Material* GetMaterialByName(const std::string& name);

	const std::unordered_map<std::string, std::shared_ptr<Material>>& GetAllMaterials() const {
		return Materials;
	}

	void AddMaterial(std::shared_ptr<Material> InMaterial) {
		Materials[InMaterial->GetName()] = std::move(InMaterial);
	}

	int InitAllMaterial();
	int DestroyAllMaterial();


private:

	std::unordered_map<std::string, std::shared_ptr<Material>> Materials;

	friend class DXRender;

};