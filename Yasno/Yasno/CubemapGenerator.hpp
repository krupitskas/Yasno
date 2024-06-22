#pragma once

namespace ysn
{
	struct Texture;

	class CubemapGenerator
	{
	public:
		void GenerateFromEquirectangular(const Texture& EquirectangularTexture);

		// Create cubemap texture
	};
}
