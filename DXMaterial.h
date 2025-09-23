#pragma once

#include <d3d12.h>

#include <wrl.h>
#include <string>
#include <unordered_map>

class Material
{

};

class MaterialManager
{
	public:
	static MaterialManager& GetInstance()
	{
		static MaterialManager instance;
		return instance;
	}


	Material& GetOrCreateMaterial(const std::string& name);


private:

	std::unordered_map<std::string, Material> Materials;

};