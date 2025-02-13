﻿module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>
#include <Windows.h>

export module renderer.dx_renderer;

import std;
import system.string_helpers;
import system.logger;
import system.asserts;
import system.helpers;
import system.compilation;
import renderer.vertex_storage;
import renderer.index_storage;
import renderer.command_queue;
import renderer.descriptor_heap;
import renderer.index_storage;
import renderer.dx_types;
import renderer.pso;
import renderer.shader_storage;

export namespace ysn
{
	class DxRenderer
	{
	public:
		bool Initialize();
		void Shutdown();

		bool CreateRootSignature(D3D12_ROOT_SIGNATURE_DESC* pRootSignatureDesc, ID3D12RootSignature** ppRootSignature) const;

		wil::com_ptr<DxDevice> GetDevice() const;

		std::optional<PsoId> BuildPso(GraphicsPsoDesc& desc);
		std::optional<PsoId> BuildPso(ComputePsoDesc& desc);
		std::optional<Pso> GetPso(PsoId pso_id);

		std::shared_ptr<ShaderStorage> GetShaderStorage()
		{
			return m_pso_storage.GetShaderStorage();
		}

		std::shared_ptr<CommandQueue> GetDirectQueue() const;
		//std::shared_ptr<CommandQueue> GetComputeQueue() const;
		//std::shared_ptr<CommandQueue> GetCopyQueue() const;

		std::shared_ptr<CbvSrvUavDescriptorHeap> GetCbvSrvUavDescriptorHeap() const;
		std::shared_ptr<SamplerDescriptorHeap> GetSamplerDescriptorHeap() const;
		std::shared_ptr<RtvDescriptorHeap> GetRtvDescriptorHeap() const;
		std::shared_ptr<DsvDescriptorHeap> GetDsvDescriptorHeap() const;

		DXGI_FORMAT GetHdrFormat() const;
		DXGI_FORMAT GetBackBufferFormat() const;
		DXGI_FORMAT GetDepthBufferFormat() const;

		void Tick()
		{
			// TODO: Better use another place?
			m_pso_storage.Tick(m_device);
		}

		void FlushQueues();

		bool IsTearingSupported();

		std::vector<wil::com_ptr<ID3D12Resource>> stage_heap;

	private:
		bool CheckRaytracingSupport();

		// Constants
		static constexpr uint32_t m_frames_count = 3;

		// Low level objects
		wil::com_ptr<DxDevice> m_device;
		wil::com_ptr<DxAdapter> m_dxgi_adapter;
		wil::com_ptr<DxSwapChain> m_swap_chain;

		// Data
		VertexStorage m_vertex_storage;
		IndexStorage m_index_storage;
		PsoStorage m_pso_storage;

		// Formats
		DXGI_FORMAT hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT backbuffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT depth_format = DXGI_FORMAT_D32_FLOAT;

		// Resources
		wil::com_ptr<ID3D12Resource> m_back_buffers[m_frames_count];

		// Queues
		std::shared_ptr<CommandQueue> m_direct_command_queue;
		//std::shared_ptr<CommandQueue> m_compute_command_queue;
		//std::shared_ptr<CommandQueue> m_copy_command_queue;

		// Descriptor heaps
		std::shared_ptr<CbvSrvUavDescriptorHeap> m_cbv_srv_uav_descriptor_heap;
		std::shared_ptr<SamplerDescriptorHeap> m_sampler_descriptor_heap;
		std::shared_ptr<RtvDescriptorHeap> m_rtv_descriptor_heap;
		std::shared_ptr<DsvDescriptorHeap> m_dsv_descriptor_heap;

		// Info

		// Get back info
		bool m_tearing_supported = false;
		bool m_is_raytracing_supported = false;
	};
}

module :private;

std::optional<wil::com_ptr<DxAdapter>> CreateDXGIAdapter(bool& allow_tearing)
{
	wil::com_ptr<IDXGIFactory5> dxgi_factory;

	UINT create_factory_flags = 0;

	if constexpr (ysn::IsDebugActive())
	{
		create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
	}

	if (auto result = CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory)); FAILED(result))
	{
		ysn::LogError << "Can't create DXGIFactory2 with flags " << std::to_string(create_factory_flags)
			<< ", error is: " << ysn::ConvertHrToString(result) << " aborting!\n";
		return std::nullopt;
	}

	wil::com_ptr<IDXGIAdapter1> dxgi_adapter;
	wil::com_ptr<DxAdapter> result_dxgi_adapter;

	SIZE_T max_dedicated_video_memory = 0;
	UINT best_adapter_index = 0;

	for (UINT i = 0; dxgi_factory->EnumAdapters1(i, dxgi_adapter.addressof()) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC3 dxgiAdapterDesc;
		result_dxgi_adapter = dxgi_adapter.try_query<DxAdapter>();

		if (!result_dxgi_adapter)
			continue;

		result_dxgi_adapter->GetDesc3(&dxgiAdapterDesc);

		ysn::LogInfo << "Found adapter " << ysn::WStringToString(dxgiAdapterDesc.Description) << "\n";

		// The adapter with the largest dedicated video memory is favored.
		if (result_dxgi_adapter && (dxgiAdapterDesc.Flags & static_cast<int>(DXGI_ADAPTER_FLAG_SOFTWARE)) == 0 &&
			SUCCEEDED(D3D12CreateDevice(dxgi_adapter.get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)) &&
			dxgiAdapterDesc.DedicatedVideoMemory > max_dedicated_video_memory)
		{
			max_dedicated_video_memory = dxgiAdapterDesc.DedicatedVideoMemory;
			best_adapter_index = i;
		}
	}

	dxgi_factory->EnumAdapters1(best_adapter_index, dxgi_adapter.addressof());

	bool tearing;
	dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));

	allow_tearing = tearing;

	return dxgi_adapter.query<DxAdapter>();
}

void MsgCallback(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void*)
{
	// Handle the debug message
	std::string dx_scope = "[Dx Debug Layer] ";
	switch (severity)
	{
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			ysn::LogFatal << dx_scope << description << "\n";
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
			ysn::LogError << dx_scope << description << "\n";
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			ysn::LogWarning << dx_scope << description << "\n";
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			ysn::LogInfo << dx_scope << description << "\n";
			break;
		default:
			ysn::LogInfo << dx_scope << " >unknown< " << description << "\n";
			break;
	}
}

std::optional<wil::com_ptr<DxDevice>> CreateDevice(wil::com_ptr<DxAdapter> adapter)
{
	wil::com_ptr<DxDevice> device;
	if (auto result = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device)); result != S_OK)
	{
		ysn::LogFatal << "Can't initialize dx 12 device " << ysn::ConvertHrToString(result) << "\n";
		return std::nullopt;
	}

	if constexpr (ysn::IsDebugActive())
	{
		wil::com_ptr<ID3D12InfoQueue1> info_queue;

		if (info_queue = device.try_query<ID3D12InfoQueue1>(); info_queue)
		{
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

			// Suppress whole categories of messages
			// D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] = { D3D12_MESSAGE_SEVERITY_INFO };

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] = {
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
			};

			D3D12_INFO_QUEUE_FILTER NewFilter = {};
			// NewFilter.DenyList.NumCategories = _countof(Categories);
			// NewFilter.DenyList.pCategoryList = Categories;
			NewFilter.DenyList.NumSeverities = _countof(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			// NewFilter.DenyList.NumIDs = _countof(DenyIds);
			// NewFilter.DenyList.pIDList = DenyIds;

			DWORD cookie = 0;
			if (auto result = info_queue->RegisterMessageCallback(&MsgCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &cookie);
				result != S_OK)
			{
				ysn::LogError << "Can't register dx debug message callback " << ysn::ConvertHrToString(result) << "\n";
			}

			if (auto result = info_queue->PushStorageFilter(&NewFilter); result != S_OK)
			{
				ysn::LogError << "Can't push storage filter to info queue" << ysn::ConvertHrToString(result) << "\n";
			}
		}
		else
		{
			ysn::LogError << "Dx Device created without Info Queue 1\n";
		}
	}

	return device;
}

namespace ysn
{
	bool DxRenderer::Initialize()
	{
		if constexpr (IsDebugActive())
		{
			wil::com_ptr<ID3D12Debug> dx_debug_interface;
			if (auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&dx_debug_interface)); result != S_OK)
			{
				LogFatal << "Can't create debug interface " << ConvertHrToString(result) << "\n";
				return false;
			}
			dx_debug_interface->EnableDebugLayer();
		}

		const auto dxgi_adapter = CreateDXGIAdapter(m_tearing_supported);

		if (!dxgi_adapter.has_value())
		{
			return false;
		}

		m_dxgi_adapter = dxgi_adapter.value();

		const auto device_result = CreateDevice(m_dxgi_adapter);

		if (!device_result.has_value())
		{
			return false;
		}

		m_device = device_result.value();

		// Create queues
		m_direct_command_queue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		//m_compute_command_queue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		//m_copy_command_queue = std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_COPY);

		// TODO(checks):
		m_direct_command_queue->Initialize();
		//m_compute_command_queue->Initialize();
		//m_copy_command_queue->Initialize();

		// TODO(checks): Save somewhere budgets, make them dynamic?
		m_cbv_srv_uav_descriptor_heap = std::make_shared<CbvSrvUavDescriptorHeap>(m_device, 1024 * 512, true);
		m_sampler_descriptor_heap = std::make_shared<SamplerDescriptorHeap>(m_device, 1024, true);
		m_rtv_descriptor_heap = std::make_shared<RtvDescriptorHeap>(m_device, 128);
		m_dsv_descriptor_heap = std::make_shared<DsvDescriptorHeap>(m_device, 128);

		// TODO(checks):
		m_cbv_srv_uav_descriptor_heap->Initialize();
		m_sampler_descriptor_heap->Initialize();
		m_rtv_descriptor_heap->Initialize();
		m_dsv_descriptor_heap->Initialize();

		m_is_raytracing_supported = CheckRaytracingSupport();

		if (!m_pso_storage.Initialize())
		{
			LogError << "Can't initialize pso storage\n";
			return false;
		}

		return true;
	}

	void DxRenderer::Shutdown()
	{
	}

	std::optional<PsoId> DxRenderer::BuildPso(GraphicsPsoDesc& pso_desc)
	{
		return m_pso_storage.BuildPso(m_device, &pso_desc);
	}

	std::optional<PsoId> DxRenderer::BuildPso(ComputePsoDesc& pso_desc)
	{
		return m_pso_storage.BuildPso(m_device, &pso_desc);
	}

	std::optional<Pso> DxRenderer::GetPso(PsoId pso_id)
	{
		return m_pso_storage.GetPso(pso_id);
	}

	bool DxRenderer::CreateRootSignature(D3D12_ROOT_SIGNATURE_DESC* pRootSignatureDesc, ID3D12RootSignature** ppRootSignature) const
	{
		HRESULT hr = S_OK;

		wil::com_ptr<ID3DBlob> pSerializeRootSignature;
		wil::com_ptr<ID3DBlob> pError;

		hr = D3D12SerializeRootSignature(
			pRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, pSerializeRootSignature.addressof(), pError.addressof());

		if (FAILED(hr))
		{
			LogError << "Can't create root signature: " << ConvertHrToString(hr) << "\n";
			return false;
		}

		if (pError)
		{
			const auto error = static_cast<char*>(pError->GetBufferPointer());
			LogError << "Can't create root signature: " << error << "\n";
			return false;
		}

		hr = m_device->CreateRootSignature(
			0, pSerializeRootSignature->GetBufferPointer(), pSerializeRootSignature->GetBufferSize(), IID_PPV_ARGS(ppRootSignature));

		if (FAILED(hr))
		{
			LogError << "Can't create root signature: " << ConvertHrToString(hr) << "\n";
			return false;
		}

		return true;
	}

	bool DxRenderer::IsTearingSupported()
	{
		return m_tearing_supported;
	}

	bool DxRenderer::CheckRaytracingSupport()
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};

		if (HRESULT result = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)); result != S_OK)
		{
			LogError << "Ray tracing is not supported";
			return false;
		}

		if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		{
			LogError << "Ray tracing tier less then 1.0";
			return false;
		}

		LogInfo << "RTX is supported\n";

		return true;
	}

	wil::com_ptr<DxDevice> DxRenderer::GetDevice() const
	{
		return m_device;
	}

	DXGI_FORMAT DxRenderer::GetHdrFormat() const
	{
		return hdr_format;
	}

	DXGI_FORMAT DxRenderer::GetBackBufferFormat() const
	{
		return backbuffer_format;
	}

	DXGI_FORMAT DxRenderer::GetDepthBufferFormat() const
	{
		return depth_format;
	}

	void DxRenderer::FlushQueues()
	{
		m_direct_command_queue->Flush();
		//m_compute_command_queue->Flush();
		//m_copy_command_queue->Flush();
	}

	std::shared_ptr<CommandQueue> DxRenderer::GetDirectQueue() const
	{
		return m_direct_command_queue;
	}

	//std::shared_ptr<CommandQueue> DxRenderer::GetComputeQueue() const
	//{
	//	return m_compute_command_queue;
	//}

	//std::shared_ptr<CommandQueue> DxRenderer::GetCopyQueue() const
	//{
	//	return m_copy_command_queue;
	//}

	std::shared_ptr<CbvSrvUavDescriptorHeap> DxRenderer::GetCbvSrvUavDescriptorHeap() const
	{
		return m_cbv_srv_uav_descriptor_heap;
	}

	std::shared_ptr<SamplerDescriptorHeap> DxRenderer::GetSamplerDescriptorHeap() const
	{
		return m_sampler_descriptor_heap;
	}

	std::shared_ptr<RtvDescriptorHeap> DxRenderer::GetRtvDescriptorHeap() const
	{
		return m_rtv_descriptor_heap;
	}

	std::shared_ptr<DsvDescriptorHeap> DxRenderer::GetDsvDescriptorHeap() const
	{
		return m_dsv_descriptor_heap;
	}

}
