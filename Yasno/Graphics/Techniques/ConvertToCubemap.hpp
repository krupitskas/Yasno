#pragma once

#include <d3d12.h>
#include <wil/com.h>

namespace ysn
{
	class CommandQueue;
	class Texture;

	class ConvertToCubemap
	{
		enum ShaderInputParameters
		{
			ViewProjection,
			InputTexture,
			ParametersCount
		};
	public:
		bool Initialize();
		void EquirectangularToCubemap(std::shared_ptr<ysn::CommandQueue> command_queue, const Texture& equirectangular_map, wil::com_ptr<ID3D12Resource> target_cubemap);
	private:

		
		wil::com_ptr<ID3D12RootSignature> m_root_signature;
		wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
	};
}
