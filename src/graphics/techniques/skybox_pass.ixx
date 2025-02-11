module;

#include <wil/com.h>
#include <d3dx12.h>

export module graphics.techniques.skybox_pass;

import std;
import graphics.primitive;
import renderer.dx_renderer;
import renderer.descriptor_heap;
import renderer.gpu_texture;
import renderer.gpu_pixel_buffer;
import renderer.command_queue;
import renderer.pso;
import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;

export namespace ysn
{
	struct SkyboxPassParameters
	{
		wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
		DescriptorHandle hdr_rtv_descriptor_handle;
		DescriptorHandle dsv_descriptor_handle;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissors_rect;
		GpuPixelBuffer3D cubemap_texture;
		wil::com_ptr<ID3D12Resource> camera_gpu_buffer;

		// TODO: debug renderer test below
		DescriptorHandle debug_counter_buffer_uav;
		DescriptorHandle debug_vertices_buffer_uav;
	};

	class SkyboxPass
	{
		enum ShaderInputParameters
		{
			CameraBuffer,
			InputTexture,
			DebugVertices,
			DebugVerticesCount,
			ParametersCount
		};

	public:
		bool Initialize();
		bool RenderSkybox(SkyboxPassParameters* parameters);

	private:
		MeshPrimitive cube;
		PsoId m_pso_id;
	};
}

module :private;

namespace ysn
{
	bool SkyboxPass::Initialize()
	{
		auto renderer = Application::Get().GetRenderer();

		const auto box_result = ConstructBox();

		if (!box_result.has_value())
		{
			return false;
		}

		cube = box_result.value();
		// CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING ???
		// A helper structure used to wrap a CD3DX12_VIEW_INSTANCING_DESC structure. Allows shaders to render to multiple views with a single draw call; useful for stereo vision or cubemap generation.

		CD3DX12_DESCRIPTOR_RANGE cubemap_srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE debug_vertices_uav(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 126, 0, 0);
		CD3DX12_DESCRIPTOR_RANGE debug_counter_uav(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 127, 0, 0);

		CD3DX12_ROOT_PARAMETER root_parameters[ShaderInputParameters::ParametersCount] = {};
		root_parameters[ShaderInputParameters::CameraBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		root_parameters[ShaderInputParameters::InputTexture].InitAsDescriptorTable(1, &cubemap_srv, D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[ShaderInputParameters::DebugVertices].InitAsDescriptorTable(1, &debug_vertices_uav, D3D12_SHADER_VISIBILITY_ALL);
		root_parameters[ShaderInputParameters::DebugVerticesCount].InitAsDescriptorTable(1, &debug_counter_uav, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_STATIC_SAMPLER_DESC static_sampler(
			0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.NumParameters = ShaderInputParameters::ParametersCount;
		root_signature_desc.pParameters = &root_parameters[0];
		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		root_signature_desc.pStaticSamplers = &static_sampler;
		root_signature_desc.NumStaticSamplers = 1;

		ID3D12RootSignature* root_signature = nullptr;

		bool result = renderer->CreateRootSignature(&root_signature_desc, &root_signature);

		if (!result)
		{
			LogError << "Can't create ConvertToCubemap root signature\n";
			return false;
		}

		D3D12_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE; // TODO: Should be back?

		D3D12_BLEND_DESC blend_desc = {};
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		GraphicsPsoDesc pso_desc("Skybox PSO");

		pso_desc.AddShader({ ShaderType::Pixel, ysn::VfsPath(L"shaders/skybox.ps.hlsl") });
		pso_desc.AddShader({ ShaderType::Vertex, ysn::VfsPath(L"shaders/skybox.vs.hlsl") });
		pso_desc.SetRootSignature(root_signature);
		pso_desc.SetRasterizerState(rasterizer_desc);
		pso_desc.SetBlendState(blend_desc);
		pso_desc.SetDepthStencilState({
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		});
		pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		pso_desc.SetSampleMask(UINT_MAX);
		pso_desc.SetRenderTargetFormat(renderer->GetHdrFormat(), renderer->GetDepthBufferFormat());
		pso_desc.SetInputLayout(Vertex::GetVertexLayoutDesc());

		auto result_pso = renderer->BuildPso(pso_desc);

		if (!result_pso.has_value())
			return false;

		m_pso_id = *result_pso;

		return true;
	}

	bool SkyboxPass::RenderSkybox(SkyboxPassParameters* parameters)
	{
		auto renderer = Application::Get().GetRenderer();

		const std::optional<Pso> pso = renderer->GetPso(m_pso_id);

		if (!pso.has_value())
		{
			LogError << "Can't find debug render PSO\n";
			return false;
		}

		auto& pso_object = pso.value();

		const auto command_list_result = renderer->GetDirectQueue()->GetCommandList("Skybox");

		if (!command_list_result.has_value())
			return false;

		auto command_list = command_list_result.value();

		ID3D12DescriptorHeap* ppHeaps[] = { renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr() };
		command_list->RSSetViewports(1, &parameters->viewport);
		command_list->RSSetScissorRects(1, &parameters->scissors_rect);
		command_list->OMSetRenderTargets(1, &parameters->hdr_rtv_descriptor_handle.cpu, FALSE, &parameters->dsv_descriptor_handle.cpu);
		command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		command_list->SetPipelineState(pso_object.pso.get());
		command_list->SetGraphicsRootSignature(pso_object.root_signature.get());
		command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list->IASetVertexBuffers(0, 1, &cube.vertex_buffer_view);

		command_list->SetGraphicsRootConstantBufferView(
			ShaderInputParameters::CameraBuffer, parameters->camera_gpu_buffer->GetGPUVirtualAddress());
		command_list->SetGraphicsRootDescriptorTable(ShaderInputParameters::InputTexture, parameters->cubemap_texture.srv.gpu);
		command_list->SetGraphicsRootDescriptorTable(ShaderInputParameters::DebugVertices, parameters->debug_vertices_buffer_uav.gpu);
		command_list->SetGraphicsRootDescriptorTable(
			ShaderInputParameters::DebugVerticesCount, parameters->debug_counter_buffer_uav.gpu);

		command_list->IASetIndexBuffer(&cube.index_buffer_view);
		command_list->DrawIndexedInstanced(cube.index_count, 1, 0, 0, 0);

		renderer->GetDirectQueue()->CloseCommandList(command_list);

		return true;
	}
}
