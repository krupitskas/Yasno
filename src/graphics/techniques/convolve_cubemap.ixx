module;

#include <d3d12.h>
#include <d3dx12.h>
#include <wil/com.h>
#include <DirectXMath.h>

export module graphics.techniques.convolve_cubemap;

import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;
import renderer.dxrenderer;
import renderer.pso;
import renderer.gpu_texture;
import renderer.gpu_pixel_buffer;
import renderer.vertex_storage;
import graphics.primitive;

export namespace ysn
{
	struct ConvolveCubemapParameters
	{
		wil::com_ptr<ID3D12Resource> camera_buffer;
		GpuPixelBuffer3D source_cubemap;
		GpuPixelBuffer3D target_irradiance;
		GpuPixelBuffer3D target_radiance;
	};

	class ConvolveCubemap
	{
	public:
		ConvolveCubemap() = default;
		bool Initialize();
		bool Render(ConvolveCubemapParameters& parameters);

	private:
		bool InitializeIrradiance();
		bool InitializeRadiance();

		bool ConvolveIrradiance(ConvolveCubemapParameters& parameters);
		bool ConvolveRadiance();

		PsoId m_irradiance_pso_id;
		PsoId m_radiance_pso_id;

		MeshPrimitive cube;

		DirectX::XMMATRIX m_projection;
		DirectX::XMMATRIX m_views[6];

		D3D12_RECT m_rect;
	};
}

module :private;

namespace ysn
{
	using namespace DirectX;

	enum IrradianceShaderParameters
	{
		ViewProjectionMatrix,
		InputTexture,
		ParametersCount
	};

	//enum RadianceShaderParameters
	//{
	//	ViewProjectionMatrix,
	//	InputTexture,
	//	ParametersCount
	//};

	bool ConvolveCubemap::Initialize()
	{
		const auto box_result = ConstructBox();
		if (!box_result.has_value())
			return false;
		cube = box_result.value();

		if (!InitializeIrradiance())
			return false;

		if (!InitializeRadiance())
			return false;

		const XMVECTOR position = XMVectorSet(0.0, 0.0, 0.0, 1.0);

		// TODO: Sus! 0 and 1 are reversed.
		m_views[0] = XMMatrixLookAtRH(position, XMVectorSet(-1.0, 0.0, 0.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
		m_views[1] = XMMatrixLookAtRH(position, XMVectorSet(1.0, 0.0, 0.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
		m_views[2] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 1.0, 0.0, 0.0), XMVectorSet(0.0, 0.0, -1.0, 0.0));
		m_views[3] = XMMatrixLookAtRH(position, XMVectorSet(0.0, -1.0, 0.0, 0.0), XMVectorSet(0.0, 0.0, 1.0, 0.0));
		m_views[4] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 0.0, 1.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
		m_views[5] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 0.0, -1.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));

		m_projection = XMMatrixPerspectiveFovRH(XMConvertToRadians(90.f), 1.0f, 0.1f, 10.f);

		m_rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

		return true;
	}

	bool ConvolveCubemap::Render(ConvolveCubemapParameters& parameters)
	{
		if (!ConvolveIrradiance(parameters))
			return false;

		return true;
	}

	bool ConvolveCubemap::InitializeIrradiance()
	{
		const auto renderer = Application::Get().GetRenderer();

		CD3DX12_DESCRIPTOR_RANGE source_texture_srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);

		CD3DX12_ROOT_PARAMETER root_parameters[IrradianceShaderParameters::ParametersCount] = {};
		root_parameters[IrradianceShaderParameters::ViewProjectionMatrix].InitAsConstants(sizeof(XMMATRIX) / sizeof(float), 0);
		root_parameters[IrradianceShaderParameters::InputTexture].InitAsDescriptorTable(1, &source_texture_srv);

		CD3DX12_STATIC_SAMPLER_DESC static_sampler(
			0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
		root_signature_desc.NumParameters = IrradianceShaderParameters::ParametersCount;
		root_signature_desc.pParameters = &root_parameters[0];
		root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		root_signature_desc.pStaticSamplers = &static_sampler;
		root_signature_desc.NumStaticSamplers = 1;

		ID3D12RootSignature* root_signature = nullptr;
		bool result = Application::Get().GetRenderer()->CreateRootSignature(&root_signature_desc, &root_signature);
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

		GraphicsPsoDesc pso_desc("Filter Irradiance Cubemap PSO");
		pso_desc.AddShader({ ShaderType::Vertex, VfsPath(L"shaders/convolve_irradiance.vs.hlsl") });
		pso_desc.AddShader({ ShaderType::Pixel, VfsPath(L"shaders/convolve_irradiance.ps.hlsl") });
		pso_desc.SetInputLayout(VertexPosTexCoord::GetVertexLayoutDesc());
		pso_desc.SetRasterizerState(rasterizer_desc);
		pso_desc.SetBlendState(blend_desc);
		pso_desc.SetRootSignature(root_signature);
		pso_desc.SetDepthStencilState({
			.DepthEnable = false,
									  });
		pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		pso_desc.SetSampleMask(UINT_MAX);
		pso_desc.SetRenderTargetFormat(renderer->GetHdrFormat(), renderer->GetDepthBufferFormat());

		auto result_pso = renderer->BuildPso(pso_desc);

		if (!result_pso.has_value())
			return false;

		m_irradiance_pso_id = *result_pso;

		return true;
	}

	bool ConvolveCubemap::InitializeRadiance()
	{

		return true;
	}

	bool ConvolveCubemap::ConvolveIrradiance(ConvolveCubemapParameters& parameters)
	{
		const auto renderer = Application::Get().GetRenderer();

		const std::optional<Pso> pso = renderer->GetPso(m_irradiance_pso_id);

		if (!pso.has_value())
		{
			LogError << "Can't find convolve irradiance PSO\n";
			return false;
		}

		auto& pso_object = pso.value();

		const auto command_list_result = renderer->GetDirectQueue()->GetCommandList("Convolve Irradiance Cubemap");

		if (!command_list_result.has_value())
			return false;

		GraphicsCommandList command_list = command_list_result.value();

		CD3DX12_RESOURCE_BARRIER barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
			parameters.target_irradiance.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		command_list.list->ResourceBarrier(1, &barrier_before);

		ID3D12DescriptorHeap* ppHeaps[] = { renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr() };

		const auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, 
							   static_cast<float>(parameters.target_irradiance->GetDesc().Width),
							   static_cast<float>(parameters.target_irradiance->GetDesc().Height));

		command_list.list->RSSetViewports(1, &viewport);
		command_list.list->RSSetScissorRects(1, &m_rect);
		command_list.list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		command_list.list->SetPipelineState(pso_object.pso.get());
		command_list.list->SetGraphicsRootSignature(pso_object.root_signature.get());
		command_list.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list.list->IASetVertexBuffers(0, 1, &cube.vertex_buffer_view);
		command_list.list->IASetIndexBuffer(&cube.index_buffer_view);

		command_list.list->SetGraphicsRootDescriptorTable(1, parameters.source_cubemap.srv.gpu);

		for (int i = 0; i < 6; i++)
		{
			DirectX::XMMATRIX view_projection = DirectX::XMMatrixIdentity();
			view_projection = XMMatrixMultiply(DirectX::XMMatrixIdentity(), m_views[i]);
			view_projection = XMMatrixMultiply(view_projection, m_projection);
			float data[16];
			XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(data), view_projection);

			command_list.list->SetGraphicsRoot32BitConstants(IrradianceShaderParameters::ViewProjectionMatrix, 16, data, 0);
			command_list.list->OMSetRenderTargets(1, &parameters.target_irradiance.rtv[i].cpu, FALSE, nullptr);
			command_list.list->DrawIndexedInstanced(cube.index_count, 1, 0, 0, 0);
		}

		CD3DX12_RESOURCE_BARRIER barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
			parameters.target_irradiance.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		command_list.list->ResourceBarrier(1, &barrier_after);

		renderer->GetDirectQueue()->ExecuteCommandList(command_list);

		return true;
	}

	bool ConvolveCubemap::ConvolveRadiance()
	{
		return true;
	}
}
