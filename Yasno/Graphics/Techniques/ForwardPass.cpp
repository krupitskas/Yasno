#include "ForwardPass.hpp"

#include <Renderer/DxRenderer.hpp>
#include <System/Application.hpp>
#include <System/Filesystem.hpp>
#include <Renderer/Pso.hpp>

namespace ysn
{
	static std::vector<DxcDefine> BuildAttributeDefines(const std::unordered_map<std::string, Attribute>& attributes)
	{
		std::vector<DxcDefine> defines;

		for (const auto& [name, attribute] : attributes)
		{
			if (attribute.name == "NORMAL")
			{
				defines.push_back({ L"HAS_NORMAL", L"1" });
			}
			else if (attribute.name == "TANGENT")
			{
				defines.push_back({ L"HAS_TANGENT", L"1" });
			}
			else if (attribute.name == "TEXCOORD_0")
			{
				defines.push_back({ L"HAS_TEXCOORD_0", L"1" });
			}
			else if (attribute.name == "TEXCOORD_1")
			{
				defines.push_back({ L"HAS_TEXCOORD_1", L"1" });
			}
		}

		return defines;
	}


	static std::vector<D3D12_INPUT_ELEMENT_DESC> BuildInputElementDescs(const std::unordered_map<std::string, Attribute>& render_attributes)
	{
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_desc_arr;

		for (const auto& [name, attribute] : render_attributes)
		{
			D3D12_INPUT_ELEMENT_DESC input_element_desc = {};

			input_element_desc.SemanticName = &attribute.name[0];
			input_element_desc.Format = attribute.format;

			// TODO: Need to parse semantic name and index from attribute name to reduce number of ifdefs
			if (attribute.name == "TEXCOORD_0")
			{
				input_element_desc.SemanticName = "TEXCOORD_";
				input_element_desc.SemanticIndex = 0;
			}

			if (attribute.name == "TEXCOORD_1")
			{
				input_element_desc.SemanticName = "TEXCOORD_";
				input_element_desc.SemanticIndex = 1;
			}

			if (attribute.name == "TEXCOORD_2")
			{
				input_element_desc.SemanticName = "TEXCOORD_";
				input_element_desc.SemanticIndex = 2;
			}

			if (attribute.name == "COLOR_0")
			{
				input_element_desc.SemanticName = "COLOR_";
				input_element_desc.SemanticIndex = 0;
			}

			if (attribute.name == "COLOR_1")
			{
				input_element_desc.SemanticName = "COLOR_";
				input_element_desc.SemanticIndex = 1;
			}

			input_element_desc.InputSlot = static_cast<UINT>(input_element_desc_arr.size());
			input_element_desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
			input_element_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

			input_element_desc_arr.push_back(input_element_desc);
		}

		return input_element_desc_arr;
	}

	bool ForwardPass::CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials)
	{
		std::shared_ptr<DxRenderer> renderer = Application::Get().GetRenderer();

		GraphicsPsoDesc new_pso_desc("Primitive PSO");

		{
			bool result = false;

			D3D12_DESCRIPTOR_RANGE shadow_input_range = {};
			shadow_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			shadow_input_range.NumDescriptors = 1;
			shadow_input_range.BaseShaderRegister = 0;

			D3D12_ROOT_PARAMETER shadow_parameter;
			shadow_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			shadow_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			shadow_parameter.DescriptorTable.NumDescriptorRanges = 1;
			shadow_parameter.DescriptorTable.pDescriptorRanges = &shadow_input_range;

			D3D12_ROOT_PARAMETER rootParams[5] = {
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_ALL },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_VERTEX },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 2, 0 }, D3D12_SHADER_VISIBILITY_ALL },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 3, 0 }, D3D12_SHADER_VISIBILITY_PIXEL },
				shadow_parameter
			};

			// TEMP
			rootParams[0].Descriptor.RegisterSpace = 0;
			rootParams[1].Descriptor.RegisterSpace = 0;
			rootParams[2].Descriptor.RegisterSpace = 0;
			rootParams[3].Descriptor.RegisterSpace = 0;

			// 0 ShadowSampler
			// 1 LinearSampler
			CD3DX12_STATIC_SAMPLER_DESC static_sampler[2];
			static_sampler[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
			static_sampler[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_COMPARISON_FUNC_NONE);

			D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
			RootSignatureDesc.NumParameters = 5;
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

		const auto primitive_attributes_defines = BuildAttributeDefines(primitive.attributes);;

		// Vertex shader
		{
			ysn::ShaderCompileParameters vs_parameters;
			vs_parameters.shader_type = ysn::ShaderType::Vertex;
			vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/ForwardPassVS.hlsl");
			vs_parameters.defines = primitive_attributes_defines;

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
			ps_parameters.defines = primitive_attributes_defines;

			const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

			if (!ps_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline ps shader\n";
				return false;
			}

			new_pso_desc.SetPixelShader(ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize());
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_desc = BuildInputElementDescs(primitive.attributes);

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

		for(auto& model : render_scene.models)
		{
			for(int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
			{
				const Mesh& mesh = model.meshes[mesh_id];
				const GpuResource& node_gpu_buffer = model.node_buffers[mesh_id];

				for(auto& primitive : mesh.primitives)
				{
					uint32_t attribute_slot = 0;
					for(const auto& [name, attribute] : primitive.attributes)
					{
						command_list->IASetVertexBuffers(attribute_slot, 1, &attribute.vertex_buffer_view);
						attribute_slot += 1;
					}

					// TODO: check for -1 as pso_id
					const std::optional<Pso> pso = renderer->GetPso(primitive.pso_id);

					if(pso.has_value())
					{
						const Material& material = model.materials[primitive.material_id];

						command_list->SetGraphicsRootSignature(pso.value().root_signature.get());
						command_list->SetPipelineState(pso.value().pso.get());

						command_list->IASetPrimitiveTopology(primitive.topology);
						command_list->SetGraphicsRootConstantBufferView(2, render_parameters.scene_parameters_gpu_buffer->GetGPUVirtualAddress());
						command_list->SetGraphicsRootConstantBufferView(3, material.gpu_material_parameters.GetGPUVirtualAddress());
						command_list->SetGraphicsRootDescriptorTable(4, render_parameters.shadow_map_buffer.srv_handle.gpu);

						command_list->SetGraphicsRootConstantBufferView(0, render_parameters.camera_gpu_buffer->GetGPUVirtualAddress());
						command_list->SetGraphicsRootConstantBufferView(1, node_gpu_buffer.GetGPUVirtualAddress());

						if (primitive.index_count)
						{
							command_list->IASetIndexBuffer(&primitive.index_buffer_view);
							command_list->DrawIndexedInstanced(primitive.index_count, 1, 0, 0, 0);
						}
						else
						{
							//ccommand_list->DrawInstanced(primitive.vertexCount, 1, 0, 0);
						}
					}
					else
					{
						LogWarning << "Can't render primitive because it has't any pso attached\n";
					}
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

		for(auto& model : render_scene.models)
		{
			for(int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
			{
				const Mesh& mesh = model.meshes[mesh_id];
				const GpuResource& node_gpu_buffer = model.node_buffers[mesh_id];

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
