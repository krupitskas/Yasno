#include "ForwardPass.hpp"

#include <Renderer/DxRenderer.hpp>
#include <System/Application.hpp>
#include <System/Filesystem.hpp>
#include <Renderer/PsoStorage.hpp>

namespace ysn
{
	//void DrawNode(ysn::ModelRenderContext* ModelRenderContext,
	//std::shared_ptr<ysn::DxRenderer> p_renderer,
	//tinygltf::Model* pModel,
	//wil::com_ptr<ID3D12GraphicsCommandList> command_list,
	//wil::com_ptr<ID3D12Resource> pCameraBuffer,
	//wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	//uint64_t nodeIndex,
	//ysn::PrimitivePipeline PrimitivePipeline,
	//ysn::ShadowMapBuffer* p_shadow_map_buffer)
	//{
	//	const auto& glTFNode = pModel->nodes[nodeIndex];

	//	if (glTFNode.mesh >= 0)
	//	{
	//		const auto& mesh = ModelRenderContext->Meshes[glTFNode.mesh];

	//		for (const ysn::Primitive& primitive : mesh.primitives)
	//		{
	//			
	//		}
	//	}

	//	for (auto childNodeIndex : glTFNode.children)
	//	{
	//		DrawNode(ModelRenderContext, p_renderer, pModel, command_list, pCameraBuffer, scene_parameters_gpu_buffer, childNodeIndex, PrimitivePipeline, p_shadow_map_buffer);
	//	}
	//}

	/*

	void ysn::RenderGLTF(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	tinygltf::Model* pModel,
	wil::com_ptr<ID3D12GraphicsCommandList> command_list,
	wil::com_ptr<ID3D12Resource> pCameraBuffer,
	wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	ysn::PrimitivePipeline PrimitivePipeline,
	ysn::ShadowMapBuffer* p_shadow_map_descriptor)
	{
	auto& scene = pModel->scenes[pModel->defaultScene];

	for (auto nodeIndex : scene.nodes)
	{
	DrawNode(ModelRenderContext, p_renderer, pModel, command_list, pCameraBuffer, scene_parameters_gpu_buffer, nodeIndex, PrimitivePipeline, p_shadow_map_descriptor);
	}
	}
	*/

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
		auto renderer = Application::Get().GetRenderer();

		GraphicsPso primitive_pso;
		primitive_pso.SetName("Primitive PSO");

		{
			bool result = false;

			D3D12_DESCRIPTOR_RANGE SrvDescriptorRange = {};
			SrvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			SrvDescriptorRange.NumDescriptors = 5;
			SrvDescriptorRange.BaseShaderRegister = 0;

			D3D12_DESCRIPTOR_RANGE DepthInputDescriptorRange = {};
			DepthInputDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			DepthInputDescriptorRange.NumDescriptors = 1;
			DepthInputDescriptorRange.BaseShaderRegister = 1 + 5;

			D3D12_ROOT_PARAMETER srv_range;
			srv_range.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			srv_range.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			srv_range.DescriptorTable.NumDescriptorRanges = 1;
			srv_range.DescriptorTable.pDescriptorRanges = &SrvDescriptorRange;

			D3D12_ROOT_PARAMETER depth_range;
			depth_range.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			depth_range.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			depth_range.DescriptorTable.NumDescriptorRanges = 1;
			depth_range.DescriptorTable.pDescriptorRanges = &DepthInputDescriptorRange;

			D3D12_ROOT_PARAMETER rootParams[6] = {
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_ALL },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_VERTEX },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 2, 0 }, D3D12_SHADER_VISIBILITY_PIXEL },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 3, 0 }, D3D12_SHADER_VISIBILITY_ALL },
				srv_range,
				depth_range
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

			primitive_pso.SetRootSignature(root_signature);
		}

		const auto primitive_attributes_defines = BuildAttributeDefines(primitive.attributes);;

		// Vertex shader
		{
			ysn::ShaderCompileParameters vs_parameters;
			vs_parameters.shader_type = ysn::ShaderType::Vertex;
			vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassVertex.hlsl");
			vs_parameters.defines = primitive_attributes_defines;

			const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

			if (!vs_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline vs shader\n";
				return false;
			}

			primitive_pso.SetVertexShader(vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize());
		}

		// Pixel shader
		{
			ysn::ShaderCompileParameters ps_parameters;
			ps_parameters.shader_type = ysn::ShaderType::Pixel;
			ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassPixelLighting.hlsl");
			ps_parameters.defines = primitive_attributes_defines;

			const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

			if (!ps_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline ps shader\n";
				return false;
			}

			primitive_pso.SetPixelShader(ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize());
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_desc = BuildInputElementDescs(primitive.attributes);

		D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		depth_stencil_desc.DepthEnable = true;
		depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		primitive_pso.SetDepthStencilState(depth_stencil_desc);
		primitive_pso.SetInputLayout(static_cast<UINT>(input_element_desc.size()), input_element_desc.data());
		primitive_pso.SetSampleMask(UINT_MAX);

		const auto material = materials[primitive.material_id];

		primitive_pso.SetRasterizerState(material.rasterizer_desc);
		primitive_pso.SetBlendState(material.blend_desc);

		switch (primitive.topology)
		{
			case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
				primitive_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
				break;
			case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
				primitive_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
				break;
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
				primitive_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
				break;
			default:
				YSN_ASSERT(false);
		}

		primitive_pso.SetRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT);

		auto result_pso = renderer->CreatePso(primitive_pso);

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

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = render_parameters.command_queue->GetCommandList();

		ID3D12DescriptorHeap* pDescriptorHeaps[] =
		{
			render_parameters.cbv_srv_uav_heap->GetHeapPtr(),
		};
		command_list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		//PIXBeginEvent(command_list.get(), PIX_COLOR_DEFAULT, "GeometryPass");

		// Clear the render targets.
		{
			FLOAT clear_color[] = { 44.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f };

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			command_list->ResourceBarrier(1, &barrier);

			command_list->ClearRenderTargetView(render_parameters.backbuffer_handle, clear_color, 0, nullptr);
			command_list->ClearDepthStencilView(render_parameters.dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			command_list->ClearRenderTargetView(render_parameters.hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);
		}

		command_list->RSSetViewports(1, &render_parameters.viewport);
		command_list->RSSetScissorRects(1, &render_parameters.scissors_rect);
		command_list->OMSetRenderTargets(1, &render_parameters.hdr_rtv_descriptor_handle.cpu, FALSE, &render_parameters.dsv_descriptor_handle.cpu);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.get(),
																					D3D12_RESOURCE_STATE_DEPTH_WRITE,
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &barrier);
		}



		for(auto& model : render_scene.models)
		{
			for(auto& mesh : model.meshes)
			{
				for(auto& primitive : mesh.primitives)
				{
					uint32_t attribute_slot = 0;
					for(const auto& [name, attribute] : primitive.attributes)
					{
						command_list->IASetVertexBuffers(attribute_slot, 1, &attribute.vertex_buffer_view);
						attribute_slot += 1;
					}

					// TODO: check for -1 as pso_id
					const GraphicsPso pso = renderer->GetPso(primitive.pso_id);

					command_list->SetGraphicsRootSignature(pso.m_root_signature.get());
					command_list->SetPipelineState(pso.m_pso.get());

					command_list->IASetPrimitiveTopology(primitive.topology);
					command_list->SetGraphicsRootConstantBufferView(2, primitive.pMaterial->pBuffer->GetGPUVirtualAddress());
					command_list->SetGraphicsRootConstantBufferView(3, scene_parameters_gpu_buffer->GetGPUVirtualAddress());
					command_list->SetGraphicsRootDescriptorTable(4, primitive.pMaterial->srv_handle.gpu);
					command_list->SetGraphicsRootDescriptorTable(5, p_shadow_map_buffer->srv_handle.gpu);

					command_list->SetGraphicsRootConstantBufferView(0, pCameraBuffer->GetGPUVirtualAddress());
					command_list->SetGraphicsRootConstantBufferView(1, ModelRenderContext->pNodeBuffers[nodeIndex]->GetGPUVirtualAddress());

					if (primitive.index_count)
					{
						command_list->IASetIndexBuffer(&primitive.index_buffer_view);
						command_list->DrawIndexedInstanced(primitive.index_count, 1, 0, 0, 0);
					}
					else
					{
						//command_list->DrawInstanced(primitive.vertexCount, 1, 0, 0);
					}
				}
			}
		}



		//switch (PrimitivePipeline)
		//{
		//	case ysn::PrimitivePipeline::ForwardPbr:
		//{

		//	break;
		//}
		//case ysn::PrimitivePipeline::Shadow:
		//	command_list->SetGraphicsRootSignature(primitive.pShadowRootSignature.get());
		//	command_list->SetPipelineState(primitive.pShadowPipelineState.get());
		//	break;
		//case ysn::PrimitivePipeline::ForwardNoMaterial:
		//	break;





		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_parameters.shadow_map_buffer.get(),
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																					D3D12_RESOURCE_STATE_DEPTH_WRITE);
			command_list->ResourceBarrier(1, &barrier);
		}

		//PIXEndEvent(command_list.get());

		render_parameters.command_queue->ExecuteCommandList(command_list);
	}

}
