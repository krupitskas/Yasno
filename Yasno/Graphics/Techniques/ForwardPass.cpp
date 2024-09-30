#include "ForwardPass.hpp"

namespace ysn
{
	/*
	

	void DrawNode(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	tinygltf::Model* pModel,
	wil::com_ptr<ID3D12GraphicsCommandList> pCommandList,
	wil::com_ptr<ID3D12Resource> pCameraBuffer,
	wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	uint64_t nodeIndex,
	ysn::PrimitivePipeline PrimitivePipeline,
	ysn::ShadowMapBuffer* p_shadow_map_buffer)
	{
	const auto& glTFNode = pModel->nodes[nodeIndex];

	if (glTFNode.mesh >= 0)
	{
	const auto& mesh = ModelRenderContext->Meshes[glTFNode.mesh];

	for (const ysn::Primitive& primitive : mesh.primitives)
	{
	pCommandList->IASetPrimitiveTopology(primitive.primitiveTopology);

	for (auto i = 0; i != primitive.RenderAttributes.size(); ++i)
	{
	pCommandList->IASetVertexBuffers(i, 1, &primitive.RenderAttributes[i].vertexBufferView);
	}

	ID3D12DescriptorHeap* pDescriptorHeaps[] = {
	p_renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr(),
	};
	pCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

	switch (PrimitivePipeline)
	{
	case ysn::PrimitivePipeline::ForwardPbr:
	{
	pCommandList->SetGraphicsRootSignature(primitive.pRootSignature.get());
	pCommandList->SetPipelineState(primitive.pPipelineState.get());
	pCommandList->SetGraphicsRootConstantBufferView(2, primitive.pMaterial->pBuffer->GetGPUVirtualAddress());
	pCommandList->SetGraphicsRootConstantBufferView(3, scene_parameters_gpu_buffer->GetGPUVirtualAddress());
	pCommandList->SetGraphicsRootDescriptorTable(4, primitive.pMaterial->srv_handle.gpu);
	pCommandList->SetGraphicsRootDescriptorTable(5, p_shadow_map_buffer->srv_handle.gpu);
	break;
	}
	case ysn::PrimitivePipeline::Shadow:
	pCommandList->SetGraphicsRootSignature(primitive.pShadowRootSignature.get());
	pCommandList->SetPipelineState(primitive.pShadowPipelineState.get());
	break;
	case ysn::PrimitivePipeline::ForwardNoMaterial:
	break;
	}

	pCommandList->SetGraphicsRootConstantBufferView(0, pCameraBuffer->GetGPUVirtualAddress());
	pCommandList->SetGraphicsRootConstantBufferView(1, ModelRenderContext->pNodeBuffers[nodeIndex]->GetGPUVirtualAddress());


	if (primitive.indexCount)
	{
	pCommandList->IASetIndexBuffer(&primitive.indexBufferView);
	pCommandList->DrawIndexedInstanced(primitive.indexCount, 1, 0, 0, 0);
	}
	else
	{
	pCommandList->DrawInstanced(primitive.vertexCount, 1, 0, 0);
	}
	}
	}

	for (auto childNodeIndex : glTFNode.children)
	{
	DrawNode(ModelRenderContext, p_renderer, pModel, pCommandList, pCameraBuffer, scene_parameters_gpu_buffer, childNodeIndex, PrimitivePipeline, p_shadow_map_buffer);
	}
	}

	void ysn::RenderGLTF(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	tinygltf::Model* pModel,
	wil::com_ptr<ID3D12GraphicsCommandList> pCommandList,
	wil::com_ptr<ID3D12Resource> pCameraBuffer,
	wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	ysn::PrimitivePipeline PrimitivePipeline,
	ysn::ShadowMapBuffer* p_shadow_map_descriptor)
	{
	auto& scene = pModel->scenes[pModel->defaultScene];

	for (auto nodeIndex : scene.nodes)
	{
	DrawNode(ModelRenderContext, p_renderer, pModel, pCommandList, pCameraBuffer, scene_parameters_gpu_buffer, nodeIndex, PrimitivePipeline, p_shadow_map_descriptor);
	}
	}
	 
	 
	bool ForwardPipeline(ysn::Primitive* pRenderPrimitive, std::shared_ptr<ysn::DxRenderer> renderer)
	{
		HRESULT result = S_OK;
		{
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
			RootSignatureDesc.NumParameters			= 6;
			RootSignatureDesc.pParameters			= &rootParams[0];
			RootSignatureDesc.Flags					= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
				| D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED // For bindless rendering
				| D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
			RootSignatureDesc.pStaticSamplers		= &static_sampler[0];
			RootSignatureDesc.NumStaticSamplers		= 2;

			result = renderer->CreateRootSignature(&RootSignatureDesc, &pRenderPrimitive->pRootSignature);
			assert(SUCCEEDED(result));
		}

		{
			ysn::ShaderCompileParameters vs_parameters;
			vs_parameters.shader_type = ysn::ShaderType::Vertex;
			vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassVertex.hlsl");
			vs_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);

			const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

			if (!vs_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline vs shader\n";
				return false;
			}

			ysn::ShaderCompileParameters ps_parameters;
			ps_parameters.shader_type = ysn::ShaderType::Pixel;
			ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassPixelLighting.hlsl");
			ps_parameters.defines = vs_parameters.defines;

			const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

			if (!ps_shader_result.has_value())
			{
				LogError << "Can't compile GLTF forward pipeline ps shader\n";
				return false;
			}

			std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs = BuildInputElementDescs(pRenderPrimitive->RenderAttributes);

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
			pipelineStateDesc.pRootSignature = pRenderPrimitive->pRootSignature.get();
			pipelineStateDesc.VS = { vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize() };
			pipelineStateDesc.PS = { ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize() };
			pipelineStateDesc.BlendState = pRenderPrimitive->pMaterial->blendDesc;
			pipelineStateDesc.SampleMask = UINT_MAX;
			pipelineStateDesc.RasterizerState = pRenderPrimitive->pMaterial->rasterizerDesc;
			pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // TODO: provide
			pipelineStateDesc.DepthStencilState.DepthEnable = true;
			pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			pipelineStateDesc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };

			switch (pRenderPrimitive->primitiveTopology)
			{
				case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
					pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
					break;
				case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
				case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
					pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
					break;
				case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
				case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
					pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
					break;
				default:
					assert(false);
			}

			pipelineStateDesc.NumRenderTargets = 1;
			pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Provide it from outside
			pipelineStateDesc.SampleDesc = { 1, 0 };

			result = renderer->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pRenderPrimitive->pPipelineState));

			assert(SUCCEEDED(result));
		}

		return true;
	}
	*/



	/*
	void ForwardPass::Render(const RenderScene& scene)
	{

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();

		PIXBeginEvent(command_list.get(), PIX_COLOR_DEFAULT, "GeometryPass");

		// Clear the render targets.
		{
			FLOAT clear_color[] = { 44.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f };

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			command_list->ResourceBarrier(1, &barrier);

			command_list->ClearRenderTargetView(backbuffer_handle, clear_color, 0, nullptr);
			command_list->ClearDepthStencilView(m_depth_dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			command_list->ClearRenderTargetView(m_hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);
		}

		command_list->RSSetViewports(1, &m_viewport);
		command_list->RSSetScissorRects(1, &m_scissors_rect);
		command_list->OMSetRenderTargets(1, &m_hdr_rtv_descriptor_handle.cpu, FALSE, &m_depth_dsv_descriptor_handle.cpu);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_pass.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &barrier);
		}

		//RenderGLTF(&m_gltf_draw_context,
		//		   Application::Get().GetRenderer(),
		//		   &m_gltf_draw_context.gltfModel,
		//		   command_list,
		//		   m_camera_gpu_buffer,
		//		   m_scene_parameters_gpu_buffer,
		//		   PrimitivePipeline::ForwardPbr,
		//		   &m_shadow_pass.shadow_map_buffer);


		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_pass.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																					D3D12_RESOURCE_STATE_DEPTH_WRITE);
			command_list->ResourceBarrier(1, &barrier);
		}

		PIXEndEvent(command_list.get());

		command_queue->ExecuteCommandList(command_list);
	}
	*/

}
