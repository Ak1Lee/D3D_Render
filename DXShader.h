#pragma once
#include <d3dcompiler.h>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <memory>




class DXShader
{
	public:

	ID3DBlob* GetBytecode();
	ID3DBlob** GetBytecodeAddress() { return ShaderByteCode.GetAddressOf(); }
	friend class DXShaderManager;

	private:

	DXShader(const std::wstring& shaderName, const std::wstring& filePath, const std::string& entryPoint, const std::string& target);

	Microsoft::WRL::ComPtr<ID3DBlob> ShaderByteCode;

	std::wstring FilePath;
	std::string EntryPoint;
	std::string Target;
	std::wstring ShaderName;

	bool bCompiled = false;
}
;

class DXShaderManager
{
	public:
	static DXShaderManager& GetInstance()
	{
		static DXShaderManager instance;
		return instance;
	}

	DXShader* CreateOrFindShader(const std::wstring& ShaderName, const std::wstring& FilePath, const std::string& EntryPoint, const std::string& Target);
	DXShader* GetShaderByName(const std::wstring& ShaderName)
	{
		auto it = ShaderCache.find(ShaderName);
		if (it != ShaderCache.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	private:
	std::unordered_map<std::wstring, std::shared_ptr<DXShader>> ShaderCache;
};