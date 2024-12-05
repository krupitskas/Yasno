#pragma once

#include <DirectXMath.h>

#include <vector>
#include <memory>

#include <Graphics/Material.hpp>
#include <Graphics/Mesh.hpp>
#include <Yasno/Lights.hpp>
#include <Renderer/GpuTexture.hpp>
#include <Renderer/GpuBuffer.hpp>
#include <Graphics/ShaderSharedStructs.h>

import yasno.camera_controller;
import yasno.camera;

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
		// Merge this two below maybe?
		std::vector<Material> materials;
		std::vector<SurfaceShaderParameters> shader_parameters;
		std::vector<NodeTransform> transforms;

		// Not sure about that
		std::vector<GpuTexture> textures;

		// Not sure about that x2
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
		GpuBuffer indices_buffer;

		// Vertices
		uint32_t vertices_count = 0;
		GpuBuffer vertices_buffer;

		// Materials
		uint32_t materials_count = 0;
		GpuBuffer materials_buffer;
		DescriptorHandle materials_buffer_srv;

		// Per instance data
		uint32_t primitives_count = 0;
		GpuBuffer instance_buffer;
		DescriptorHandle instance_buffer_srv;
	};
}

