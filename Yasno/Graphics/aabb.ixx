export module graphics.aabb;
export import <DirectXMath.h>;

export namespace ysn
{
	struct AABB
	{
		DirectX::XMFLOAT3 min;
		DirectX::XMFLOAT3 max;
	};
}
