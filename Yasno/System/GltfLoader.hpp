#pragma once

#include <DirectXMath.h>

#include <Graphics/RenderScene.hpp>

namespace ysn
{
	struct LoadingParameters
	{
		DirectX::XMMATRIX model_modifier = DirectX::XMMatrixIdentity(); // Applies matrix modifier for nodes and RTX BVH generation
	};

	bool LoadGltfFromFile(RenderScene& render_scene, const std::wstring& path, const LoadingParameters& loading_parameters);
}
