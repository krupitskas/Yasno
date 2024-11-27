#pragma once

#include <cstdint>
#include <memory>

#include <DirectXMath.h>

#include <Renderer/DxRenderer.hpp>

namespace ysn
{
	struct GpuTexture;

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
		bool Initialize(const DxRenderer& renderer);
		bool GenerateMips(std::shared_ptr<DxRenderer> renderer, wil::com_ptr<ID3D12GraphicsCommandList> command_list, const GpuTexture& gpu_texture);
	private:
		wil::com_ptr<ID3D12RootSignature> m_root_signature;
		wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
	};
}
