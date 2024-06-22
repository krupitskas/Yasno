#pragma once

#include <cstdint>
#include <memory>

#include <DirectXMath.h>

#include <RHI/D3D12Renderer.hpp>

namespace ysn
{
	class Texture;

	SHADER_STRUCT GenerateMipsConstantBuffer
	{
		uint32_t src_mip_level;			// Texture level of source mip
		uint32_t num_mip_levels;		// Number of OutMips to write: [1-4]
		uint32_t src_dimension;			// Width and height of the source texture are even or odd.
		uint32_t is_srgb;				// Must apply gamma correction to sRGB textures.
		DirectX::XMFLOAT2 texel_size;	// 1.0 / OutMip1.Dimensions

		uint32_t pad[2];
	};

	namespace RootSignatureIndex
	{
		enum
		{
			GenerateMipsCB,
			SrcMip,
			OutMip,
			NumParameters
		};
	}

	class GenerateMipsSystem
	{
	public:
		bool Initialize(const D3D12Renderer& renderer);
		bool GenerateMips(std::shared_ptr<D3D12Renderer> renderer, wil::com_ptr<ID3D12GraphicsCommandList> command_list, const Texture& gpu_texture);
	private:
		wil::com_ptr<ID3D12RootSignature> m_root_signature;
		wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
	};

}
