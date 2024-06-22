#pragma once

#include <vector>

class RenderSceneNode
{

};

class RenderScene
{
public:
	std::vector<RenderSceneNode> nodes;

	bool SaveToDisk();
	bool LoadFromDisk();
};
