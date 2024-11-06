#pragma once

#include <DirectXMath.h>

namespace ysn
{
	struct AABB
	{
		DirectX::XMFLOAT3 min;
		DirectX::XMFLOAT3 max;
	};
}
