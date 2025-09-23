#include "DXMaterial.h"

Material& MaterialManager::GetOrCreateMaterial(const std::string& name)
{
    // TODO: 在此处插入 return 语句

    if(Materials.find(name) == Materials.end())
    {
        Materials[name] = Material();
	}

	return Materials[name];
}
