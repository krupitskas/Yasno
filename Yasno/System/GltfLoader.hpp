#pragma once

#include <unordered_map>
#include <memory>
#include <unordered_set>

#include <d3d12.h>
#include <d3dx12.h>
#include <wil/com.h>
#include <tiny_gltf.h>
#include <DirectXMath.h>

#include <Renderer/DescriptorHeap.hpp>
#include <Renderer/GpuTexture.hpp>
#include <Graphics/RenderScene.hpp>

namespace ysn
{
	struct LoadingParameters
	{
		DirectX::XMMATRIX model_modifier = DirectX::XMMatrixIdentity(); // Applies matrix modifier for nodes and RTX BVH generation
		bool capture_pix = false; // Capture command list with PIX and save to disk
	};

	bool LoadGltfFromFile(RenderScene& render_scene, const std::wstring& path, const LoadingParameters& loading_parameters);
}
