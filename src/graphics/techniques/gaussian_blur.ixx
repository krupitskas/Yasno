module;

#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

export module graphics.techniques.gaussian_blur;

import graphics.primitive;
import renderer.dx_renderer;
import renderer.descriptor_heap;
import renderer.vertex_storage;
import renderer.gpu_buffer;
import renderer.dx_types;
import renderer.command_queue;
import renderer.pso;
import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;

namespace ysn
{
	enum class GaussianBlurType
	{
		Normal, 
		Bilateral
	};

	struct GaussianBlurInput
	{
		//ShadowMapBuffer shadow_map_buffer;
		//DescriptorHandle hdr_uav_descriptor_handle;
		//DescriptorHandle depth_srv;

		//wil::com_ptr<ID3D12Resource> camera_gpu_buffer;
		//wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer;
		//wil::com_ptr<ID3D12Resource> scene_color_buffer;

		uint32_t backbuffer_width = 0;
		uint32_t backbuffer_height = 0;
	};

	class GaussianBlur
	{
	public:
		bool Initialize();
		bool Render(const GaussianBlurInput& pass_input);
		void PrepareData(wil::com_ptr<DxGraphicsCommandList> cmd_list, uint32_t width, uint32_t height);

		GpuBuffer gpu_parameters;
	private:

	};

}

module :private;

namespace ysn
{
	void GaussianBlur::PrepareData(wil::com_ptr<DxGraphicsCommandList> cmd_list, uint32_t width, uint32_t height)
	{
	}

	bool GaussianBlur::Initialize()
	{
		return false;
	}

	bool GaussianBlur::Render(const GaussianBlurInput& pass_input)
	{
		return false;
	}
}