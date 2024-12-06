export module graphics.mesh;

import std;
import graphics.primitive;

namespace ysn
{
	struct Mesh
	{
		std::string name = "Unnamed Mesh";

		std::vector<Primitive> primitives;
	};
}
