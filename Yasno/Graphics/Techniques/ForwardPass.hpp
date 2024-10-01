#pragma once

#include <Graphics/RenderScene.hpp>

namespace ysn
{
	struct ForwardPass
	{
		void Initialize();
		bool CompilePso(Primitive& primitive);
		void Render(const RenderScene& scene);
	};
}
