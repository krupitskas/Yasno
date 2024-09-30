#pragma once

#include <DirectXMath.h>

#include <vector>
#include <memory>

#include <Graphics/Material.hpp>
#include <Graphics/Mesh.hpp>
#include <Graphics/RenderObjectId.hpp>
#include <Yasno/Lights.hpp>
#include <Yasno/CameraController.hpp>
#include <Renderer/GpuResource.hpp>
#include <Renderer/GpuTexture.hpp>

namespace ysn
{
	struct NodeTransformation
	{
		DirectX::XMFLOAT4X4 transform;
	};

	struct ModelRenderParameters
	{
		bool should_cast_shadow		= false;

	};

	struct Model
	{
		std::string name			= "Unnamed Model";
		ModelRenderParameters render_parameters;

		std::vector<Mesh> meshes;

		// Not sure about that
		std::vector<GpuResource> buffers;
		std::vector<GpuTexture> textures;

		// Not sure about that x2
		std::vector<GpuResource> node_buffers; // Per node GPU data
		std::vector<D3D12_SAMPLER_DESC> sampler_descs;
	};
	
	struct RenderScene
	{
		std::shared_ptr<Camera> camera;
		FpsCameraController camera_controler;

		DirectionalLight directional_light;
		EnvironmentLight environment_light;

		std::vector<Model> models;
	};
}

