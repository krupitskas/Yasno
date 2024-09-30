#pragma once

#include <Graphics/Primitive.hpp>

namespace ysn
{
	struct Mesh
	{
		std::string name = "Unnamed Mesh";

		std::vector<Primitive> primitives;
	};
}
