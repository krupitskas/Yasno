#include "ShadowMapPass.hpp"


#include <Renderer/DxRenderer.hpp>
#include <System/Math.hpp>
#include <Yasno/Lights.hpp>

namespace ysn
{

	/*
	bool ShadowPipeline(ysn::Primitive* pRenderPrimitive, std::shared_ptr<ysn::DxRenderer> renderer)
	{
		HRESULT result = S_OK;

		{
			D3D12_ROOT_PARAMETER rootParams[2] = {
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_VERTEX },
				{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_VERTEX }
			};

			// TEMP
			rootParams[0].Descriptor.RegisterSpace = 0;
			rootParams[1].Descriptor.RegisterSpace = 0;

			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.NumParameters = _countof(rootParams);
			rootSignatureDesc.pParameters = &rootParams[0];
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			result = renderer->CreateRootSignature(&rootSignatureDesc, &pRenderPrimitive->pShadowRootSignature);

			assert(SUCCEEDED(result));
		}

		{
			ysn::ShaderCompileParameters vs_parameters;
			vs_parameters.shader_type = ysn::ShaderType::Vertex;
			vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassVertex.hlsl");
			vs_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);
			vs_parameters.defines.emplace_back(L"SHADOW_PASS");

			const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

			if (!vs_shader_result.has_value())
			{
				LogError << "Can't compile GLTF shadow pipeline vs shader\n";
				return false;
			}

			ysn::ShaderCompileParameters ps_parameters;
			ps_parameters.shader_type = ysn::ShaderType::Pixel;
			ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassPixelGray.hlsl");
			ps_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);

			const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

			if (!ps_shader_result.has_value())
			{
				LogError << "Can't compile GLTF shadow pipeline ps shader\n";
				return false;
			}

			auto inputElementDescs = BuildInputElementDescs(pRenderPrimitive->RenderAttributes);

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
			pipelineStateDesc.pRootSignature = pRenderPrimitive->pShadowRootSignature.get();
			pipelineStateDesc.VS = { vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize() };
			pipelineStateDesc.PS = { ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize() };
			pipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			pipelineStateDesc.SampleMask = UINT_MAX;
			pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // TODO: Why front?
			pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // TODO: provide
			pipelineStateDesc.DepthStencilState.DepthEnable = true;
			pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
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

			result = renderer->GetDevice()->CreateGraphicsPipelineState(
				&pipelineStateDesc, IID_PPV_ARGS(&pRenderPrimitive->pShadowPipelineState));

			assert(SUCCEEDED(result));
		}

		return true;
	}
	*/

	bool ShadowMapPass::InitializeCamera(std::shared_ptr<DxRenderer> p_renderer)
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = AlignPow2(sizeof(ShadowCamera), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		auto result = p_renderer->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pCameraBuffer));

		if (result != S_OK)
		{
			LogFatal << "Can't create camera GPU buffer\n";
			return false;
		}

		return true;
	}

	void ShadowMapPass::Initialize(std::shared_ptr<DxRenderer> p_renderer)
	{
		InitializeShadowMapBuffer(p_renderer);
		//InitializeOrthProjection();
		InitializeCamera(p_renderer);
	}

	bool ShadowMapPass::InitializeShadowMapBuffer(std::shared_ptr<DxRenderer> p_renderer)
	{
		HRESULT result = S_OK;
		const DXGI_FORMAT ShadowMapFormat = DXGI_FORMAT_D32_FLOAT;

		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = ShadowMapFormat;
		optimizedClearValue.DepthStencil = { 0.0f, 0 };

		const CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(ShadowMapFormat, ShadowMapDimension, ShadowMapDimension, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		result = p_renderer->GetDevice()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&shadow_map_buffer.buffer));

		if (result != S_OK)
			return false;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = ShadowMapFormat;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_view_desc = {};
		srv_view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_view_desc.Texture2D.MipLevels = 1;

		shadow_map_buffer.dsv_handle = p_renderer->GetDsvDescriptorHeap()->GetNewHandle();
		shadow_map_buffer.srv_handle = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

		p_renderer->GetDevice()->CreateDepthStencilView(shadow_map_buffer.buffer.get(), &dsv, shadow_map_buffer.dsv_handle.cpu);
		p_renderer->GetDevice()->CreateShaderResourceView(shadow_map_buffer.buffer.get(), &srv_view_desc, shadow_map_buffer.srv_handle.cpu);

		Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(ShadowMapDimension), static_cast<float>(ShadowMapDimension));
		ScissorRect = CD3DX12_RECT(0, 0, ShadowMapDimension, ShadowMapDimension);

		return true;
	}

	void ShadowMapPass::InitializeOrthProjection(DirectX::SimpleMath::Vector3 direction)
	{
		DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicOffCenterLH(-100.0f, 100.0f, -100.0f, 100.0f, 0.1f, 100.f);
		DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(-direction, DirectX::SimpleMath::Vector3::Zero, DirectX::SimpleMath::Vector3::Up);

		shadow_matrix = view * projection;
	}

	void ShadowMapPass::UpdateLight(const DirectionalLight& directional_light)
	{
		InitializeOrthProjection(SimpleMath::Vector3{ directional_light.direction.x, directional_light.direction.y, directional_light.direction.z });
	}

	/*

	void ShadowMapPass::Render(
		std::shared_ptr<DxRenderer> p_renderer,
		std::shared_ptr<CommandQueue> command_queue,
		wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
		ModelRenderContext* pGLTFDrawContext,
		tinygltf::Model* )
	{
		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList("ShadowMap");

		command_list->ClearDepthStencilView(shadow_map_buffer.dsv_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
		command_list->RSSetViewports(1, &Viewport);
		command_list->RSSetScissorRects(1, &ScissorRect);
		command_list->OMSetRenderTargets(0, nullptr, FALSE, &shadow_map_buffer.dsv_handle.cpu);

		void* pData;
		m_pCameraBuffer->Map(0, nullptr, &pData);

		auto* pCameraData = static_cast<ShadowCamera*>(pData);

		XMStoreFloat4x4(&pCameraData->shadow_matrix, shadow_matrix);

		m_pCameraBuffer->Unmap(0, nullptr);

		RenderGLTF(pGLTFDrawContext, p_renderer, &pGLTFDrawContext->gltfModel, command_list, m_pCameraBuffer, scene_parameters_gpu_buffer, PrimitivePipeline::Shadow, &shadow_map_buffer);

		command_queue->ExecuteCommandList(command_list);
	}

	*/
} // namespace ysn
