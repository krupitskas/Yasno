#include "CascadedShadowMaps.hpp"

#include <d3dx12.h>

#include <RHI/D3D12Renderer.hpp>
#include <RHI/GpuMarker.hpp>
#include <System/Math.hpp>
#include <Yasno/Lights.hpp>

namespace ysn
{
	bool ysn::CascadedShadowMaps::InitializeCamera(std::shared_ptr<ysn::D3D12Renderer> p_renderer)
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

	void CascadedShadowMaps::Initialize(std::shared_ptr<ysn::D3D12Renderer> p_renderer)
	{
		InitializeShadowMapBuffer(p_renderer);
		//InitializeOrthProjection();
		InitializeCamera(p_renderer);
	}

	bool CascadedShadowMaps::InitializeShadowMapBuffer(std::shared_ptr<ysn::D3D12Renderer> p_renderer)
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

	void CascadedShadowMaps::InitializeOrthProjection(DirectX::SimpleMath::Vector3 direction)
	{
		DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicOffCenterLH(-100.0f, 100.0f, -100.0f, 100.0f, 0.1f, 100.f);
		DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(-direction, DirectX::SimpleMath::Vector3::Zero, DirectX::SimpleMath::Vector3::Up);

		shadow_matrix = view * projection;
	}

	void CascadedShadowMaps::UpdateLight(const DirectionalLight& directional_light)
	{
		InitializeOrthProjection(SimpleMath::Vector3{ directional_light.direction.x, directional_light.direction.y, directional_light.direction.z });
	}

	void CascadedShadowMaps::Render(
		std::shared_ptr<ysn::D3D12Renderer> p_renderer,
		std::shared_ptr<ysn::CommandQueue> command_queue,
		wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
		ysn::ModelRenderContext* pGLTFDrawContext,
		tinygltf::Model* )
	{
		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();

		GpuMarker shadow_map_marker(command_list, "ShadowMap");

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

		shadow_map_marker.EndEvent();

		command_queue->ExecuteCommandList(command_list);
	}
} // namespace ysn
