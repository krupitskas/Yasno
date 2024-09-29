#pragma once

#include <DirectXMath.h>

#include <vector>
#include <memory>

#include <Graphics/Material.hpp>
#include <Graphics/Mesh.hpp>
#include <Graphics/RenderObjectId.hpp>
#include <Yasno/Lights.hpp>
#include <Yasno/CameraController.hpp>

namespace ysn
{
	struct NodeTransformation
	{
		DirectX::XMFLOAT4X4 model;
	};

	// Model desciption info
	struct Model
	{
		std::string name			= "Unnamed Model";
		bool should_cast_shadow		= false;
	};

	struct RenderScene
	{
		std::shared_ptr<Camera> camera;
		FpsCameraController camera_controler;

		DirectionalLight directional_light;
		EnvironmentLight environment_light;

		std::vector<Model> models;
		std::vector<NodeTransformation> transformations;
		std::vector<Mesh> meshes;
		std::vector<Material> materials;

		RenderObjectId AddObject();

		// Switches pointers between current object and last one, decrement num_objects and deletes it later.
		void DeleteObject(RenderObjectId object);

		uint32_t GetModelCount() const
		{
			return num_objects;
		}
		
	private:
		// Keep track of live objects to render
		uint32_t num_objects = 0;
	};
}

