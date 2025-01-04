module;

#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

export module graphics.techniques.debug_renderer;

import graphics.primitive;
import renderer.dxrenderer;
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
	struct IndirectCommand
	{
		D3D12_GPU_VIRTUAL_ADDRESS camera_parameters_cbv;
		D3D12_DRAW_ARGUMENTS draw_arguments;
	};
}

export namespace ysn
{
	struct DebugRendererParameters
	{
		GraphicsCommandList cmd_list;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissors_rect;
		wil::com_ptr<ID3D12Resource> camera_gpu_buffer;
	};

	class DebugRenderer
	{
	public:
		bool Initialize(wil::com_ptr<DxGraphicsCommandList> cmd_list, wil::com_ptr<ID3D12Resource> camera_gpu_buffer);
		bool RenderDebugGeometry(DebugRendererParameters& parameters);

		GpuBuffer counter_buffer;
		DescriptorHandle counter_uav;

		GpuBuffer vertices_buffer;
		DescriptorHandle vertices_buffer_uav;

	private:
		GpuBuffer m_counter_buffer_zero;

		wil::com_ptr<ID3D12CommandSignature> m_command_signature;
		GpuBuffer m_command_buffer;
		PsoId m_pso_id;
	};

}

module :private;

namespace ysn
{
	enum ShaderInputParameters
	{
		CameraBuffer,
		ParametersCount
	};

	bool DebugRenderer::Initialize(wil::com_ptr<DxGraphicsCommandList> cmd_list, wil::com_ptr<ID3D12Resource> camera_gpu_buffer)
	{
		auto renderer = Application::Get().GetRenderer();

		{
			const uint32_t vertex_buffer_size = g_max_debug_render_vertices_count * sizeof(DebugRenderVertex);

			GpuBufferCreateInfo create_info{
				.size = vertex_buffer_size, .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, .state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS };

			const auto result = CreateGpuBuffer(create_info, "Debug Renderer Vertices");

			if (!result.has_value())
			{
				LogError << "Can't create debug renderer vertices buffer\n";
				return false;
			}

			vertices_buffer = result.value();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = g_max_debug_render_vertices_count;
			uav_desc.Buffer.CounterOffsetInBytes = 0;
			uav_desc.Buffer.StructureByteStride = sizeof(DebugRenderVertex);

			vertices_buffer_uav = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			renderer->GetDevice()->CreateUnorderedAccessView(vertices_buffer.Resource(), nullptr, &uav_desc, vertices_buffer_uav.cpu);
		}

		{
			GpuBufferCreateInfo create_info{
				.size = sizeof(uint32_t), .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, .state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS };

			const auto result = CreateGpuBuffer(create_info, "Debug Line Counter");

			if (!result.has_value())
			{
				LogError << "Can't create debug line counter buffer\n";
				return false;
			}

			counter_buffer = result.value();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = 1;
			uav_desc.Buffer.CounterOffsetInBytes = 0; // TODO: use it
			uav_desc.Buffer.StructureByteStride = sizeof(uint32_t);

			counter_uav = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			renderer->GetDevice()->CreateUnorderedAccessView(counter_buffer.Resource(), nullptr, &uav_desc, counter_uav.cpu);
		}

		{
			GpuBufferCreateInfo create_info{
				.size = sizeof(uint32_t), .heap_type = D3D12_HEAP_TYPE_UPLOAD, .state = D3D12_RESOURCE_STATE_GENERIC_READ };

			const auto result = CreateGpuBuffer(create_info, "Debug Line Counter Zero");

			if (!result.has_value())
			{
				LogError << "Can't create debug line counter zero buffer\n";
				return false;
			}

			m_counter_buffer_zero = result.value();

			UINT zero = 0;
			void* mapped_data = nullptr;
			m_counter_buffer_zero.Resource()->Map(0, nullptr, &mapped_data);
			memcpy(mapped_data, &zero, sizeof(zero));
			m_counter_buffer_zero.Resource()->Unmap(0, nullptr);
		}

		CD3DX12_ROOT_PARAMETER root_parameters[ShaderInputParameters::ParametersCount] = {};
		root_parameters[ShaderInputParameters::CameraBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.NumParameters = ShaderInputParameters::ParametersCount;
		root_signature_desc.pParameters = &root_parameters[0];
		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3D12RootSignature* root_signature = nullptr;

		if (!renderer->CreateRootSignature(&root_signature_desc, &root_signature))
		{
			LogError << "Can't create ConvertToCubemap root signature\n";
			return false;
		}

		D3D12_RASTERIZER_DESC rasterizer_desc = {};
		rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE; // TODO: Should be back?

		D3D12_BLEND_DESC blend_desc = {};
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		GraphicsPsoDesc pso_desc("Debug Render PSO");
		pso_desc.SetRootSignature(root_signature);
		pso_desc.AddShader({ ShaderType::Vertex, VfsPath(L"shaders/debug_geometry.vs.hlsl") });
		pso_desc.AddShader({ ShaderType::Pixel, VfsPath(L"shaders/debug_geometry.ps.hlsl") });
		pso_desc.SetRasterizerState(rasterizer_desc);
		pso_desc.SetBlendState(blend_desc);
		pso_desc.SetDepthStencilState({ .DepthEnable = false });
		pso_desc.SetSampleMask(UINT_MAX);
		pso_desc.SetInputLayout(DebugRenderVertex::GetVertexLayoutDesc());
		pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
		pso_desc.SetRenderTargetFormat(renderer->GetBackBufferFormat(), renderer->GetDepthBufferFormat());

		auto result_pso = renderer->BuildPso(pso_desc);

		if (!result_pso.has_value())
			return false;

		m_pso_id = *result_pso;

		// Create command signature
		D3D12_INDIRECT_ARGUMENT_DESC argument_desc[2] = {};
		argument_desc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argument_desc[0].ConstantBufferView.RootParameterIndex = 0;
		argument_desc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

		D3D12_COMMAND_SIGNATURE_DESC command_signature_desc = {};
		command_signature_desc.pArgumentDescs = argument_desc;
		command_signature_desc.NumArgumentDescs = _countof(argument_desc);
		command_signature_desc.ByteStride = sizeof(IndirectCommand);

		if (auto result =
				renderer->GetDevice()->CreateCommandSignature(&command_signature_desc, root_signature, IID_PPV_ARGS(&m_command_signature));
			result != S_OK)
		{
			LogError << "Debug render pass can't create command signature\n";
			return false;
		}

		std::vector<IndirectCommand> indirect_commands;

		for (int i = 0; i < g_max_debug_render_commands_count; i++)
		{
			IndirectCommand command;
			command.camera_parameters_cbv = camera_gpu_buffer->GetGPUVirtualAddress();
			command.draw_arguments.VertexCountPerInstance = 2;
			command.draw_arguments.InstanceCount = 1;
			command.draw_arguments.StartInstanceLocation = 0;
			command.draw_arguments.StartVertexLocation = i * 2;

			indirect_commands.push_back(command);
		}

		GpuBufferCreateInfo create_info{
			.size = g_max_debug_render_commands_count * sizeof(IndirectCommand),
			.heap_type = D3D12_HEAP_TYPE_DEFAULT,
			.state = D3D12_RESOURCE_STATE_COPY_DEST };

		const auto command_buffer_result = CreateGpuBuffer(create_info, "Debug Command Buffer");

		if (!command_buffer_result.has_value())
		{
			LogError << "Can't create debug command buffer\n";
			return false;
		}

		m_command_buffer = command_buffer_result.value();

		UploadToGpuBuffer(cmd_list, m_command_buffer, indirect_commands.data(), {}, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		return true;
	}

	bool DebugRenderer::RenderDebugGeometry(DebugRendererParameters& parameters)
	{
		auto renderer = Application::Get().GetRenderer();

		const std::optional<Pso> pso = renderer->GetPso(m_pso_id);

		if (!pso.has_value())
		{
			LogError << "Can't find debug render PSO\n";
			return false;
		}

		auto& pso_object = pso.value();

		ID3D12DescriptorHeap* pDescriptorHeaps[] = {
			renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr(),
		};

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		vertex_buffer_view.BufferLocation = vertices_buffer.GPUVirtualAddress();
		vertex_buffer_view.StrideInBytes = sizeof(DebugRenderVertex);
		vertex_buffer_view.SizeInBytes = g_max_debug_render_vertices_count * sizeof(DebugRenderVertex);

		parameters.cmd_list.list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);
		parameters.cmd_list.list->RSSetViewports(1, &parameters.viewport);
		parameters.cmd_list.list->RSSetScissorRects(1, &parameters.scissors_rect);
		parameters.cmd_list.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		parameters.cmd_list.list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

		parameters.cmd_list.list->SetPipelineState(pso_object.pso.get());
		parameters.cmd_list.list->SetGraphicsRootSignature(pso_object.root_signature.get());
		parameters.cmd_list.list->ExecuteIndirect(
			m_command_signature.get(), g_max_debug_render_commands_count, m_command_buffer.Resource(), 0, counter_buffer.Resource(), 0);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				counter_buffer.Resource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST);
			parameters.cmd_list.list->ResourceBarrier(1, &barrier);
		}

		// Zero out counter
		parameters.cmd_list.list->CopyBufferRegion(counter_buffer.Resource(), 0, m_counter_buffer_zero.Resource(), 0, sizeof(UINT));

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				counter_buffer.Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			parameters.cmd_list.list->ResourceBarrier(1, &barrier);
		}

		return true;
	}
}