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
	private:

		
		wil::com_ptr<ID3D12RootSignature> m_root_signature;
		wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
	};
}
