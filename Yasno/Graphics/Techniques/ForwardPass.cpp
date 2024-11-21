#include "ForwardPass.hpp"

#include <Renderer/DxRenderer.hpp>
#include <System/Application.hpp>
#include <System/Filesystem.hpp>
#include <Renderer/Pso.hpp>

namespace ysn
{
	//static std::vector<D3D12_INPUT_ELEMENT_DESC> BuildInputElementDescs(const std::unordered_map<std::string, Attribute>& render_attributes)
	//{
	//	std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_desc_arr;

	//	for (const auto& [name, attribute] : render_attributes)
	//	{
	//		D3D12_INPUT_ELEMENT_DESC input_element_desc = {};

	//		input_element_desc.SemanticName = &attribute.name[0];
	//		input_element_desc.Format = attribute.format;

	//		// TODO: Need to parse semantic name and index from attribute name to reduce number of ifdefs
	//		if (attribute.name == "TEXCOORD_0")
	//		{
	//			input_element_desc.SemanticName = "TEXCOORD_";
	//			input_element_desc.SemanticIndex = 0;
	//		}

	//		if (attribute.name == "TEXCOORD_1")
	//		{
	//			input_element_desc.SemanticName = "TEXCOORD_";
	//			input_element_desc.SemanticIndex = 1;
	//		}

	//		if (attribute.name == "TEXCOORD_2")
	//		{
	//			input_element_desc.SemanticName = "TEXCOORD_";
	//			input_element_desc.SemanticIndex = 2;
	//		}

	//		if (attribute.name == "COLOR_0")
	//		{
	//			input_element_desc.SemanticName = "COLOR_";
	//			input_element_desc.SemanticIndex = 0;
	//		}

	//		if (attribute.name == "COLOR_1")
	//		{
	//			input_element_desc.SemanticName = "COLOR_";
	//			input_element_desc.SemanticIndex = 1;
	//		}

	//		input_element_desc.InputSlot = static_cast<UINT>(input_element_desc_arr.size());
	//		input_element_desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//		input_element_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	//		input_element_desc_arr.push_back(input_element_desc);
	//	}

	//	return input_element_desc_arr;
	//}

	bool ForwardPass::CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials)
	{
		std::shared_ptr<DxRenderer> renderer = Application::Get().GetRenderer();

		GraphicsPsoDesc new_pso_desc("Primitive PSO");

		{
			bool result = false;

			D3D12_DESCRIPTOR_RANGE instance_data_input_range = {};
			instance_data_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			instance_data_input_range.NumDescriptors = 1;
			instance_data_input_range.BaseShaderRegister = 0;

			D3D12_ROOT_PARAMETER instance_buffer_parameter;
			instance_buffer_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			instance_buffer_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			instance_buffer_parameter.DescriptorTable.NumDescriptorRanges = 1;
			instance_buffer_parameter.DescriptorTable.pDescriptorRanges = &instance_data_input_range;

			D3D12_DESCRIPTOR_RANGE materials_input_range = {};
			materials_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			materials_input_range.NumDescriptors = 1;
			materials_input_range.BaseShaderRegister = 1;

			D3D12_ROOT_PARAMETER materials_buffer_parameter;
			materials_buffer_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			materials_buffer_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			materials_buffer_parameter.DescriptorTable.NumDescriptorRanges = 1;
			materials_buffer_parameter.DescriptorTable.pDescriptorRanges = &materials_input_range;

			D3D12_DESCRIPTOR_RANGE shadow_input_range = {};
			shadow_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			shadow_input_range.NumDescriptors = 1;
			shadow_input_range.BaseShaderRegister = 2;

			D3D12_ROOT_PARAMETER shadow_parameter;
			shadow_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			shadow_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			shadow_parameter.DescriptorTable.NumDescriptorRanges = 1;
			shadow_parameter.DescriptorTable.pDescriptorRanges = &shadow_input_range;

			D3D12_ROOT_PARAMETER rootParams[6] = {
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_ALL }, // CameraParameters
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_ALL }, // SceneParameters

				// InstanceID
				{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
					.Constants = {
						.ShaderRegister = 2,
						.RegisterSpace = 0,
						.Num32BitValues = 1
					},
					.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
				},

				instance_buffer_parameter, // PerInstanceData
				materials_buffer_parameter, // Materials buffer
				shadow_parameter	// Shadow Map input
			};

			// TEMP
			rootParams[0].Descriptor.RegisterSpace = 0;
			rootParams[1].Descriptor.RegisterSpace = 0;

			// 0 ShadowSampler
			// 1 LinearSampler
			CD3DX12_STATIC_SAMPLER_DESC static_sampler[2];
			static_sampler[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
			static_sampler[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_COMPARISON_FUNC_NONE);

			D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
			RootSignatureDesc.NumParameters = 6;
			RootSignatureDesc.pParameters = &rootParams[0];
			RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
				| D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED // For bindless rendering
				| D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
			RootSignatureDesc.pStaticSamplers = &static_sampler[0];
			RootSignatureDesc.NumStaticSamplers = 2;

			ID3D12RootSignature* root_signature = nullptr;

			result = renderer->CreateRootSignature(&RootSignatureDesc, &root_signature);

			if (!result)
			{
				LogFatal << "Can't create root signature for primitive\n";
				return false;
			}

			new_pso_desc.SetRootSignature(root_signature);
		}

		// Vertex shader
		{
			ysn::ShaderCompileParameters vs_parameters;
			vs_parameters.shader_type = ysn::ShaderType::Vertex;
			vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/ForwardPassVS.hlsl");

			const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

			if (!vs_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline vs shader\n";
				return false;
			}

			new_pso_desc.SetVertexShader(vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize());
		}

		// Pixel shader
		{
			ysn::ShaderCompileParameters ps_parameters;
			ps_parameters.shader_type = ysn::ShaderType::Pixel;
			ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/ForwardPassPS.hlsl");

			const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

			if (!ps_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline ps shader\n";
				return false;
			}

			new_pso_desc.SetPixelShader(ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize());
		}

		const auto& input_element_desc = renderer->GetInputElementsDesc();

		D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		depth_stencil_desc.DepthEnable = true;
		depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		new_pso_desc.SetDepthStencilState(depth_stencil_desc);
		new_pso_desc.SetInputLayout(static_cast<UINT>(input_element_desc.size()), input_element_desc.data());
		new_pso_desc.SetSampleMask(UINT_MAX);

		const auto material = materials[primitive.material_id];

		new_pso_desc.SetRasterizerState(material.rasterizer_desc);
		new_pso_desc.SetBlendState(material.blend_desc);

		switch (primitive.topology)
		{
			case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
				new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
				break;
			case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
				new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
				break;
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
				new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
				break;
			default:
				YSN_ASSERT(false);
		}

		new_pso_desc.SetRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT);

		auto result_pso = renderer->CreatePso(new_pso_desc);

		if (!result_pso.has_value())
		{
			return false;
		}

		primitive.pso_id = *result_pso;

		return true;
	}

	void ForwardPass::Render(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters)
	{
		auto renderer = Application::Get().GetRenderer();

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = render_parameters.command_queue->GetCommandList("Forward Pass");

		ID3D12DescriptorHeap* pDescriptorHeaps[] =
		{
			render_parameters.cbv_srv_uav_heap->GetHeapPtr(),
		};
		command_list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);
		command_list->RSSetViewports(1, &render_parameters.viewport);
		command_list->RSSetScissorRects(1, &render_parameters.scissors_rect);
		command_list->OMSetRenderTargets(1, &render_parameters.hdr_rtv_descriptor_handle.cpu, FALSE, &render_parameters.dsv_descriptor_handle.cpu);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_DEPTH_WRITE,
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &barrier);
		}

		uint32_t instance_id = 0;

		for(auto& model : render_scene.models)
		{
			for(int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
			{
				const Mesh& mesh = model.meshes[mesh_id];

				for(int primitive_id = 0; primitive_id < mesh.primitives.size(); primitive_id++)
				{
					const Primitive& primitive = mesh.primitives[primitive_id];

					command_list->IASetVertexBuffers(0, 1, &primitive.vertex_buffer_view);

					// TODO: check for -1 as pso_id
					const std::optional<Pso> pso = renderer->GetPso(primitive.pso_id);

					if(pso.has_value())
					{
						const Material& material = model.materials[primitive.material_id];

						command_list->SetGraphicsRootSignature(pso.value().root_signature.get());
						command_list->SetPipelineState(pso.value().pso.get());

						command_list->IASetPrimitiveTopology(primitive.topology);

						command_list->SetGraphicsRootConstantBufferView(0, render_parameters.camera_gpu_buffer->GetGPUVirtualAddress());
						command_list->SetGraphicsRootConstantBufferView(1, render_parameters.scene_parameters_gpu_buffer->GetGPUVirtualAddress());
						command_list->SetGraphicsRoot32BitConstant(2, instance_id, 0); // InstanceID

						command_list->SetGraphicsRootDescriptorTable(3, render_scene.instance_buffer_srv.gpu);
						command_list->SetGraphicsRootDescriptorTable(4, render_scene.materials_buffer_srv.gpu);
						command_list->SetGraphicsRootDescriptorTable(5, render_parameters.shadow_map_buffer.srv_handle.gpu);

						if (primitive.index_count)
						{
							command_list->IASetIndexBuffer(&primitive.index_buffer_view);
							command_list->DrawIndexedInstanced(primitive.index_count, 1, 0, 0, 0);
						}
						else
						{
							command_list->DrawInstanced(primitive.vertex_count, 1, 0, 0);
						}
					}
					else
					{
						LogWarning << "Can't render primitive because it haven't any PSO\n";
					}

					instance_id++;
				}
			}
		}

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																					D3D12_RESOURCE_STATE_DEPTH_WRITE);
			command_list->ResourceBarrier(1, &barrier);
		}


		render_parameters.command_queue->ExecuteCommandList(command_list);
	}

	// Data structure to match the command signature used for ExecuteIndirect.
	struct IndirectCommand
	{
		D3D12_GPU_VIRTUAL_ADDRESS camera_parameters_cbv;
		D3D12_GPU_VIRTUAL_ADDRESS scene_parameters_cbv;
		D3D12_DRAW_INDEXED_ARGUMENTS draw_arguments;
	};

	void Initialize()
	{
		// Each command consists of a CBV update and a DrawInstanced call.
		D3D12_INDIRECT_ARGUMENT_DESC argument_desc[3] = {};

		argument_desc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argument_desc[0].ConstantBufferView.RootParameterIndex = 0;

		argument_desc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argument_desc[1].ConstantBufferView.RootParameterIndex = 1;

		argument_desc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC command_sig_desc = {};
		command_sig_desc.pArgumentDescs = argument_desc;
		command_sig_desc.NumArgumentDescs = _countof(argument_desc);
		command_sig_desc.ByteStride = sizeof(IndirectCommand);

		//ThrowIfFailed(m_device->CreateCommandSignature(&command_sig_desc, m_rootSignature.Get(), IID_PPV_ARGS(&m_commandSignature)));
	}

	void ForwardPass::RenderIndirect(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters)
	{
		auto renderer = Application::Get().GetRenderer();

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = render_parameters.command_queue->GetCommandList("Forward Pass Indirect");

		ID3D12DescriptorHeap* pDescriptorHeaps[] =
		{
			render_parameters.cbv_srv_uav_heap->GetHeapPtr(),
		};
		command_list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);
		command_list->RSSetViewports(1, &render_parameters.viewport);
		command_list->RSSetScissorRects(1, &render_parameters.scissors_rect);
		command_list->OMSetRenderTargets(1, &render_parameters.hdr_rtv_descriptor_handle.cpu, FALSE, &render_parameters.dsv_descriptor_handle.cpu);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_DEPTH_WRITE,
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &barrier);
		}

		for(const ysn::Model& model : render_scene.models)
		{
			//command_list->IASetIndexBuffer(&primitive.index_buffer_view);
			//command_list->IASetVertexBuffers(0, 1, &m_vertexBufferView);
			//command_list->IASetPrimitiveTopology(primitive.topology);
			//command_list->SetGraphicsRootSignature(pso.value().root_signature.get());
			//command_list->SetPipelineState(pso.value().pso.get());
			
			//	for(const auto& [name, attribute] : primitive.attributes)
			//	{
			//		command_list->IASetVertexBuffers(attribute_slot, 1, &attribute.vertex_buffer_view);
			//		attribute_slot += 1;

			for(int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
			{
				const Mesh& mesh = model.meshes[mesh_id];
				//const GpuResource& node_gpu_buffer = model.node_buffers[mesh_id];

				if (m_enable_culling)
				{
					// later	
				}
				else
				{
					//command_list->ExecuteIndirect();
				}

				//for(auto& primitive : mesh.primitives)
				//{
				//	uint32_t attribute_slot = 0;
				//	for(const auto& [name, attribute] : primitive.attributes)
				//	{
				//		command_list->IASetVertexBuffers(attribute_slot, 1, &attribute.vertex_buffer_view);
				//		attribute_slot += 1;
				//	}

				//	// TODO: check for -1 as pso_id
				//	const std::optional<Pso> pso = renderer->GetPso(primitive.pso_id);

				//	if(pso.has_value())
				//	{
				//		const Material& material = model.materials[primitive.material_id];

				//		command_list->SetGraphicsRootSignature(pso.value().root_signature.get());
				//		command_list->SetPipelineState(pso.value().pso.get());

				//		command_list->IASetPrimitiveTopology(primitive.topology);
				//		command_list->SetGraphicsRootConstantBufferView(2, render_parameters.scene_parameters_gpu_buffer->GetGPUVirtualAddress());
				//		command_list->SetGraphicsRootConstantBufferView(3, material.gpu_material_parameters.GetGPUVirtualAddress());
				//		command_list->SetGraphicsRootDescriptorTable(4, render_parameters.shadow_map_buffer.srv_handle.gpu);

				//		command_list->SetGraphicsRootConstantBufferView(0, render_parameters.camera_gpu_buffer->GetGPUVirtualAddress());
				//		command_list->SetGraphicsRootConstantBufferView(1, node_gpu_buffer.GetGPUVirtualAddress());

				//		if (primitive.index_count)
				//		{
				//			command_list->IASetIndexBuffer(&primitive.index_buffer_view);
				//			command_list->DrawIndexedInstanced(primitive.index_count, 1, 0, 0, 0);
				//		}
				//		else
				//		{
				//			//command_list->DrawInstanced(primitive.vertexCount, 1, 0, 0);
				//		}
				//	}
				//	else
				//	{
				//		LogWarning << "Can't render primitive because it has't any pso attached\n";
				//	}
				//}
			}
		}

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																					D3D12_RESOURCE_STATE_DEPTH_WRITE);
			command_list->ResourceBarrier(1, &barrier);
		}


		render_parameters.command_queue->ExecuteCommandList(command_list);
	}

}
