module;

#include <DirectXMath.h>

export module graphics.aabb;

export namespace ysn
{
	struct AABB
	{
		DirectX::XMFLOAT3 min;
		DirectX::XMFLOAT3 max;
	};
}
