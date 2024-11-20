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
#include <Renderer/GpuBuffer.hpp>

namespace ysn
{
	struct NodeTransform
	{
		NodeTransform() = default;
		NodeTransform(const DirectX::XMMATRIX& transform) : transform(transform) {}

		DirectX::XMMATRIX transform;
	};

	struct ModelRenderParameters
	{
		bool should_cast_shadow = false;
	};

	struct Model
	{
		std::string name = "Unnamed Model";
		ModelRenderParameters render_parameters;

		std::vector<Mesh> meshes;
		std::vector<Material> materials;
		std::vector<NodeTransform> transforms;

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

		// Temp GPU resources for indices and vertices
		uint32_t indices_count = 0;
		uint32_t vertices_count = 0;
		GpuBuffer indices_buffer;
		GpuBuffer vertices_buffer;
	};
}

