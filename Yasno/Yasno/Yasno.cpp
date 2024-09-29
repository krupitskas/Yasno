#include "Yasno.hpp"

#include <algorithm>

#include <d3dcompiler.h>
#include <d3dx12.h>
#include <pix3.h>

#include <System/Application.hpp>
#include <Renderer/CommandQueue.hpp>
#include <System/Helpers.hpp>
#include <System/Window.hpp>
#include <System/GltfLoader.hpp>
#include <System/Gui.hpp>
#include <Renderer/DxRenderer.hpp>
#include <System/Filesystem.hpp>
#include <System/Assert.hpp>
#include <System/Filesystem.hpp>
#include <System/Math.hpp>
#include <Graphics/EngineStats.hpp>

namespace ysn
{
	uint32_t GpuSceneParameters::GetGpuSize()
	{
		return static_cast<uint32_t>(ysn::AlignPow2(sizeof(GpuSceneParameters), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	}

	std::optional<wil::com_ptr<ID3D12Resource>> CreateCubemapTexture()
	{
		wil::com_ptr<ID3D12Resource> cubemap_gpu_texture;

		auto device = Application::Get().GetDevice();

		const auto size = 512;
		const auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		D3D12_RESOURCE_DESC texture_desc = {};
		texture_desc.Format				 = format;
		texture_desc.Width				 = size;
		texture_desc.Height				 = size;
		texture_desc.Flags				 = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		texture_desc.DepthOrArraySize	 = 6;
		texture_desc.SampleDesc.Count	 = 1;
		texture_desc.SampleDesc.Quality	 = 0;
		texture_desc.Dimension			 = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		const auto heap_properties_default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		//const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		if(device->CreateCommittedResource(
			&heap_properties_default,
			D3D12_HEAP_FLAG_NONE,
			&texture_desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&cubemap_gpu_texture)) != S_OK)
		{
			LogError << "Can't create cubemap resource\n";
			return std::nullopt;
		}

	#ifndef YSN_RELEASE
		cubemap_gpu_texture->SetName(L"Cubemap Texture");
	#endif

		return cubemap_gpu_texture;
	}

	Yasno::Yasno(const std::wstring& name, int width, int height, bool vSync) :
		Game(name, width, height, vSync),
		m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
		m_scissors_rect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	{}

	void Yasno::UnloadContent()
	{
		m_is_content_loaded = false;
	}

	void Yasno::Destroy()
	{
		Game::Destroy();
		ShutdownImgui();
	}

	void Yasno::UpdateBufferResource(
		wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
		ID3D12Resource** destination_resource,
		ID3D12Resource** intermediate_resource,
		size_t num_elements,
		size_t element_size,
		const void* buffer_data,
		D3D12_RESOURCE_FLAGS flags)
	{
		auto device = Application::Get().GetDevice();

		size_t bufferSize = num_elements * element_size;

		const auto gpuBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
		const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		// Create a committed resource for the GPU resource in a default heap.
		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&gpuBufferDesc,
			D3D12_RESOURCE_STATE_COMMON, // TODO(task): should be here
			// D3D12_RESOURCE_STATE_COPY_DEST?
			nullptr,
			IID_PPV_ARGS(destination_resource)));

		// Create an committed resource for the upload.
		if (buffer_data)
		{
			const auto gpuUploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			const auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &gpuUploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(intermediate_resource)));

			D3D12_SUBRESOURCE_DATA subresource_ata = {};
			subresource_ata.pData = buffer_data;
			subresource_ata.RowPitch = bufferSize;
			subresource_ata.SlicePitch = subresource_ata.RowPitch;

			UpdateSubresources(commandList.get(), *destination_resource, *intermediate_resource, 0, 0, 1, &subresource_ata);
		}
	}

	bool Yasno::CreateGpuCameraBuffer()
	{
		wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = GpuCamera::GetGpuSize();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		auto hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_camera_gpu_buffer));

		if (hr != S_OK) 
		{
			LogFatal << "Can't create camera GPU buffer\n";
			return false;
		}

		return true;
	}

	bool Yasno::CreateGpuSceneParametersBuffer()
	{
		wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = GpuSceneParameters::GetGpuSize();
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		auto hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_scene_parameters_gpu_buffer));

		if (hr != S_OK) 
		{
			LogFatal << "Can't create scene parameters GPU buffer\n";
			return false;
		}

		return true;
	}

	void Yasno::UpdateGpuCameraBuffer()
	{
		void* data;
		m_camera_gpu_buffer->Map(0, nullptr, &data);

		auto* camera_data = static_cast<GpuCamera*>(data);

		DirectX::XMMATRIX view_projection = DirectX::XMMatrixIdentity();
		view_projection = XMMatrixMultiply(DirectX::XMMatrixIdentity(), m_render_scene.camera->GetViewMatrix());
		view_projection = XMMatrixMultiply(view_projection, m_render_scene.camera->GetProjectionMatrix());
		
		DirectX::XMVECTOR det;

		XMStoreFloat4x4(&camera_data->view_projection, view_projection);
		XMStoreFloat4x4(&camera_data->view, m_render_scene.camera->GetViewMatrix());
		XMStoreFloat4x4(&camera_data->projection, m_render_scene.camera->GetProjectionMatrix());
		XMStoreFloat4x4(&camera_data->view_inverse, XMMatrixInverse(&det, m_render_scene.camera->GetViewMatrix()));
		XMStoreFloat4x4(&camera_data->projection_inverse, XMMatrixInverse(&det, m_render_scene.camera->GetProjectionMatrix()));

		camera_data->position = m_render_scene.camera->GetPosition();

		m_camera_gpu_buffer->Unmap(0, nullptr);
	}

	void Yasno::UpdateGpuSceneParametersBuffer()
	{
		void* data;
		m_scene_parameters_gpu_buffer->Map(0, nullptr, &data);

		auto* scene_parameters_data = static_cast<GpuSceneParameters*>(data);

		XMStoreFloat4x4(&scene_parameters_data->shadow_matrix, m_shadow_pass.shadow_matrix);
		scene_parameters_data->directional_light_color = m_render_scene.directional_light.color;
		scene_parameters_data->directional_light_direction = m_render_scene.directional_light.direction;
		scene_parameters_data->directional_light_intensity = m_render_scene.directional_light.intensity;
		scene_parameters_data->ambient_light_intensity = m_render_scene.environment_light.intensity;
		scene_parameters_data->shadows_enabled = (uint32_t)m_render_scene.directional_light.cast_shadow;

		m_scene_parameters_gpu_buffer->Unmap(0, nullptr);
	}

	bool Yasno::LoadContent()
	{
		wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();
		auto renderer = Application::Get().GetRenderer();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0; // TODO: WHY NOT D3D_ROOT_SIGNATURE_VERSION_1_2?????

		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// auto command_queue = Application::Get().GetCopyQueue();
		auto command_queue = Application::Get().GetDirectQueue();

		//PIXCaptureParameters pix_capture_parameters;
		//pix_capture_parameters.GpuCaptureParameters.FileName = L"Yasno.pix";
		//PIXSetTargetWindow(m_pWindow->GetWindowHandle());
		//YSN_ASSERT(PIXBeginCapture(PIX_CAPTURE_GPU, &pix_capture_parameters) != S_OK);

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();

		LoadingParameters loading_parameters;

		// TODO(last): Another command list?
		//LoadGLTFModel(&m_gltf_draw_context, GetVirtualFilesystemPath(L"Assets/DamagedHelmet/DamagedHelmet.gltf"), Application::Get().GetRenderer(), command_list);
		bool load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"Assets/Sponza/Sponza.gltf"), loading_parameters);
		bool load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"Assets/DamagedHelmet/DamagedHelmet.gltf"), loading_parameters);
		//LoadGLTFModel(&m_gltf_draw_context, GetVirtualFilesystemPath(L"Assets/BoomBoxWithAxes/glTF/BoomBoxWithAxes.gltf"), Application::Get().GetRenderer(), command_list);
		//LoadGLTFModel(&m_gltf_draw_context, GetVirtualFilesystemPath(L"Assets/Bistro/Bistro.gltf"), Application::Get().GetRenderer(), command_list);
		//LoadGLTFModel(&m_gltf_draw_context, GetVirtualFilesystemPath(L"Assets/Sponza_New/NewSponza_Main_glTF_002.gltf"), Application::Get().GetRenderer(), command_list);

		if(!m_tonemap_pass.Initialize())
		{
			LogFatal << "Can't initialize tonemap pass\n";
			return false;
		}

		if(!m_convert_to_cubemap_pass.Initialize())
		{
			LogFatal << "Can't initialize convert to cubemap pass\n";
			return false;
		}

		if(!m_skybox_pass.Initialize())
		{
			LogFatal << "Can't initialize skybox pass\n";
			return false;
		}

		InitializeImgui(m_pWindow, renderer);

		const auto enviroment_hdr_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
		m_environment_texture = LoadHDRTextureFromFile("assets/HDRI/drackenstein_quarry_puresky_4k.hdr", command_list, device, enviroment_hdr_descriptor_handle);

		m_hdr_uav_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
		m_backbuffer_uav_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
		m_depth_dsv_descriptor_handle = renderer->GetDsvDescriptorHeap()->GetNewHandle();

		m_raytracing_context.CreateAccelerationStructures(Application::Get().GetRenderer()->GetDevice(), command_list, &m_render_scene);

		if(!CreateGpuCameraBuffer())
		{
			LogFatal << "Yasno app can't create GPU camera buffer\n";
			return false;
		}

		if(!CreateGpuSceneParametersBuffer())
		{
			LogFatal << "Yasno app can't create GPU scene parameters buffer\n";
			return false;
		}

		auto fence_value = command_queue->ExecuteCommandList(command_list);
		command_queue->WaitForFenceValue(fence_value);

		//YSN_ASSERT(PIXEndCapture(false) != S_OK);

		m_is_content_loaded = true;

		// Resize/Create the depth buffer.
		ResizeDepthBuffer(GetClientWidth(), GetClientHeight());
		ResizeHdrBuffer(GetClientWidth(), GetClientHeight());
		ResizeBackBuffer(GetClientWidth(), GetClientHeight());

		// Setup techniques
		m_shadow_pass.Initialize(Application::Get().GetRenderer());

		if (!m_ray_tracing_pass.Initialize(Application::Get().GetRenderer(), m_scene_color_buffer, m_raytracing_context, m_camera_gpu_buffer))
		{
			LogFatal << "Can't initialize raytracing pass\n";
			return false;
		}

		// Setup camera
		m_render_scene.camera = std::make_shared<ysn::Camera>();
		m_render_scene.camera->SetPosition({ -5, 0, 0 });
		m_render_scene.camera_controler.pCamera = m_render_scene.camera;

		game_input.Initialize(m_pWindow->GetWindowHandle());

		// Post init
		const auto cubemap_texture_result = CreateCubemapTexture();
		if(!cubemap_texture_result.has_value())
		{
			LogFatal << "Can't create cubemap\n";
			return false;
		}

		m_cubemap_texture = cubemap_texture_result.value();

		return true;
	}

	void Yasno::ResizeDepthBuffer(int width, int height)
	{
		if (m_is_content_loaded)
		{
			// Flush any GPU commands that might be referencing the depth buffer.
			Application::Get().Flush();

			width = std::max(1, width);
			height = std::max(1, height);

			wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

			// Resize screen dependent resources.
			// Create a depth buffer.
			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			optimizedClearValue.DepthStencil = { 1.0f, 0 };

			const CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

			ThrowIfFailed(device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&optimizedClearValue,
				IID_PPV_ARGS(&m_depth_buffer)));

			// Update the depth-stencil view.
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
			dsv.Format = DXGI_FORMAT_D32_FLOAT;
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Texture2D.MipSlice = 0;
			dsv.Flags = D3D12_DSV_FLAG_NONE;

			device->CreateDepthStencilView(m_depth_buffer.get(), &dsv, m_depth_dsv_descriptor_handle.cpu);
		}
	}

	void Yasno::ResizeHdrBuffer(int width, int height)
	{
		if (m_is_content_loaded)
		{
			// Flush any GPU commands that might be referencing the depth buffer.
			Application::Get().Flush();

			width = std::max(1, width);
			height = std::max(1, height);

			auto device = Application::Get().GetDevice();
			auto renderer = Application::Get().GetRenderer();

			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = Application::Get().GetRenderer()->GetHdrFormat();
			optimizedClearValue.Color[0] = 0.0f;
			optimizedClearValue.Color[1] = 0.0f;
			optimizedClearValue.Color[2] = 0.0f;
			optimizedClearValue.Color[3] = 0.0f;

			const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				Application::Get().GetRenderer()->GetHdrFormat(), width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			ThrowIfFailed(device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&optimizedClearValue,
				IID_PPV_ARGS(&m_scene_color_buffer)));

		#ifndef YSN_RELEASE
			m_scene_color_buffer->SetName(L"Scene Color");
		#endif

			D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
			rtv.Format = Application::Get().GetRenderer()->GetHdrFormat();
			rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv.Texture2D.MipSlice = 0;

			m_hdr_rtv_descriptor_handle = renderer->GetRtvDescriptorHeap()->GetNewHandle();

			device->CreateRenderTargetView(m_scene_color_buffer.get(), &rtv, m_hdr_rtv_descriptor_handle.cpu);

			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = Application::Get().GetRenderer()->GetHdrFormat();
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				device->CreateUnorderedAccessView(m_scene_color_buffer.get(), nullptr, &uavDesc, m_hdr_uav_descriptor_handle.cpu);
			}
		}
	}

	void Yasno::ResizeBackBuffer(int width, int height)
	{
		if (m_is_content_loaded)
		{
			// Flush any GPU commands that might be referencing the depth buffer.
			Application::Get().Flush();

			width = std::max(1, width);
			height = std::max(1, height);

			auto device = Application::Get().GetDevice();

			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = Application::Get().GetRenderer()->GetBackBufferFormat();
			optimizedClearValue.Color[0] = 0.0f;
			optimizedClearValue.Color[1] = 0.0f;
			optimizedClearValue.Color[2] = 0.0f;
			optimizedClearValue.Color[3] = 0.0f;

			const auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				Application::Get().GetRenderer()->GetBackBufferFormat(), width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			ThrowIfFailed(device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				&optimizedClearValue,
				IID_PPV_ARGS(&m_back_buffer)));

		#ifndef YSN_RELEASED
			m_back_buffer->SetName(L"Back Buffer");
		#endif

			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = Application::Get().GetRenderer()->GetBackBufferFormat();
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				device->CreateUnorderedAccessView(m_back_buffer.get(), nullptr, &uavDesc, m_backbuffer_uav_descriptor_handle.cpu);
			}
		}
	}

	void Yasno::OnResize(ResizeEventArgs& e)
	{
		if (e.Width != GetClientWidth() || e.Height != GetClientHeight())
		{
			Game::OnResize(e);

			m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

			ResizeDepthBuffer(e.Width, e.Height);
			ResizeHdrBuffer(e.Width, e.Height);
			ResizeBackBuffer(e.Width, e.Height);
		}
	}

	void Yasno::OnUpdate(UpdateEventArgs& e)
	{
		static uint64_t frameCount = 0;
		static double totalTime = 0.0;

		Game::OnUpdate(e);

		totalTime += e.ElapsedTime;
		frameCount++;

		if (totalTime > 1.0)
		{
			engine_stats::fps = uint32_t(frameCount / totalTime);

			frameCount = 0;
			totalTime = 0.0;
		}

		auto kb = game_input.keyboard->GetState();

		auto mouse = game_input.mouse->GetState();
		game_input.mouse->SetMode(mouse.rightButton ? DirectX::Mouse::MODE_RELATIVE : DirectX::Mouse::MODE_ABSOLUTE);

		if (mouse.positionMode == DirectX::Mouse::MODE_RELATIVE)
		{
			m_render_scene.camera_controler.MoveMouse(mouse.x, mouse.y);

			m_render_scene.camera_controler.m_IsMovementBoostActive = kb.IsKeyDown(DirectX::Keyboard::LeftShift);

			if (kb.IsKeyDown(DirectX::Keyboard::W))
			{
				m_render_scene.camera_controler.MoveForward(static_cast<float>(e.ElapsedTime));
			}
			if (kb.IsKeyDown(DirectX::Keyboard::A))
			{
				m_render_scene.camera_controler.MoveLeft(static_cast<float>(e.ElapsedTime));
			}
			if (kb.IsKeyDown(DirectX::Keyboard::S))
			{
				m_render_scene.camera_controler.MoveBackwards(static_cast<float>(e.ElapsedTime));
			}
			if (kb.IsKeyDown(DirectX::Keyboard::D))
			{
				m_render_scene.camera_controler.MoveRight(static_cast<float>(e.ElapsedTime));
			}
			if (kb.IsKeyDown(DirectX::Keyboard::Space))
			{
				m_render_scene.camera_controler.MoveUp(static_cast<float>(e.ElapsedTime));
			}
			if (kb.IsKeyDown(DirectX::Keyboard::LeftControl))
			{
				m_render_scene.camera_controler.MoveDown(static_cast<float>(e.ElapsedTime));
			}
		}

		if (kb.IsKeyDown(DirectX::Keyboard::Escape))
		{
			Application::Get().Quit(0);
		}

		{
			if (kb.IsKeyDown(DirectX::Keyboard::R) && !m_is_raster_pressed)
			{
				m_is_raster = !m_is_raster;
				m_is_raster_pressed = true;
			}

			if (kb.IsKeyUp(DirectX::Keyboard::R))
			{
				m_is_raster_pressed = false;
			}
		}

		m_render_scene.camera->SetAspectRatio(GetClientWidth() / static_cast<float>(GetClientHeight()));
		m_render_scene.camera->Update();

	#ifndef YSN_RELEASE
		// Track shader updates
		Application::Get().GetRenderer()->GetShaderStorage()->VerifyAnyShaderChanged();
	#endif
	}

	void Yasno::RenderUi()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit", "Esc"))
				{
					Application::Get().Quit(0);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		{
			ImGui::Begin(CONTROLS_NAME);

			if (ImGui::CollapsingHeader("Lighting"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				static float color[3] = { m_render_scene.directional_light.color.x, m_render_scene.directional_light.color.y, m_render_scene.directional_light.color.z };
				ImGui::ColorEdit3("Color", color, ImGuiColorEditFlags_Float);
				m_render_scene.directional_light.color.x = color[0];
				m_render_scene.directional_light.color.y = color[1];
				m_render_scene.directional_light.color.z = color[2];

				static float dir[3] = { m_render_scene.directional_light.direction.x, m_render_scene.directional_light.direction.y, m_render_scene.directional_light.direction.z };
				ImGui::InputFloat3("Direction", dir);
				m_render_scene.directional_light.direction.x = dir[0];
				m_render_scene.directional_light.direction.y = dir[1];
				m_render_scene.directional_light.direction.z = dir[2];

				m_shadow_pass.UpdateLight(m_render_scene.directional_light);

				ImGui::InputFloat("Intensity", &m_render_scene.directional_light.intensity, 0.0f, 1000.0f);
				ImGui::InputFloat("Env Light Intensity", &m_render_scene.environment_light.intensity, 0.0f, 1000.0f);
			}

			if (ImGui::CollapsingHeader("Shadows"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::Checkbox("Enabled",  &m_render_scene.directional_light.cast_shadow);
			}

			if (ImGui::CollapsingHeader("Tonemapping"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				const char* items[] = { "None", "Reinhard", "ACES", };
				static int item_current = (int)m_tonemap_pass.tonemap_method;
				ImGui::Combo("Tonemapper", &item_current, items, IM_ARRAYSIZE(items));
				m_tonemap_pass.tonemap_method = static_cast<TonemapMethod>(item_current);

				ImGui::InputFloat("Exposure", &m_tonemap_pass.exposure, 0.0, 1000.0);
			}

			if (ImGui::CollapsingHeader("Camera"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::InputFloat("Speed", &m_render_scene.camera_controler.mouse_speed, 0.0f, 1000.0f);
				ImGui::InputFloat("FOV", &m_render_scene.camera->fov, 0.0f, 1000.0f);
			}

			if (ImGui::CollapsingHeader("System"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				ImGui::Checkbox("Vsync",  &m_vSync);
			}

			ImGui::End();
		}

		{
			ImGui::Begin(STATS_NAME);

			ImGui::Text(std::format("FPS: {}", engine_stats::fps).c_str());

			if (ImGui::CollapsingHeader("Mode"), ImGuiTreeNodeFlags_DefaultOpen)
			{
				if(m_is_raster)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
					ImGui::Text("Rasterization");
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 0, 255));
					ImGui::Text("Pathtracing");
					ImGui::PopStyleColor();
				}
			}

			ImGui::End();
		}
	}

	void Yasno::OnRender(RenderEventArgs& e)
	{
		Game::OnRender(e);

		// Render imgui
		{
			ImguiPrepareNewFrame();
			RenderUi();
		}

		std::shared_ptr<ysn::CommandQueue> command_queue = Application::Get().GetDirectQueue();
		auto renderer = Application::Get().GetRenderer();

		UINT current_backbuffer_index = m_pWindow->GetCurrentBackBufferIndex();
		wil::com_ptr<ID3D12Resource> current_back_buffer = m_pWindow->GetCurrentBackBuffer();
		D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_handle = m_pWindow->GetCurrentRenderTargetView();

		m_convert_to_cubemap_pass.EquirectangularToCubemap(command_queue, m_environment_texture, m_cubemap_texture);

		m_shadow_pass.Render(Application::Get().GetRenderer(), command_queue, m_scene_parameters_gpu_buffer, &m_render_scene);

		UpdateGpuCameraBuffer();
		UpdateGpuSceneParametersBuffer();

		if (m_is_raster)
		{
			//m_forward_pass.Render(Application::Get().GetRenderer(), command_queue, m_scene_parameters_gpu_buffer, &m_render_scene);
		}
		else
		{
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();

			PIXBeginEvent(command_list.get(), PIX_COLOR_DEFAULT, "RTX Pass");

			// Clear the render targets.
			{
				FLOAT clear_color[] = { 255.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f };

				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

				command_list->ResourceBarrier(1, &barrier);
				command_list->ClearRenderTargetView(backbuffer_handle, clear_color, 0, nullptr);
				command_list->ClearDepthStencilView(m_depth_dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
				command_list->ClearRenderTargetView(m_hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);
			}

			m_ray_tracing_pass.Execute(Application::Get().GetRenderer(), command_list, GetClientWidth(), GetClientHeight(), m_scene_color_buffer, m_camera_gpu_buffer);

			PIXEndEvent(command_list.get());

			command_queue->ExecuteCommandList(command_list);
		}

		if (m_is_raster)
		{
			SkyboxPassParameters skybox_parameters;
			skybox_parameters.command_queue = command_queue;
			skybox_parameters.cbv_srv_uav_heap = renderer->GetCbvSrvUavDescriptorHeap();
			skybox_parameters.scene_color_buffer = m_scene_color_buffer;
			skybox_parameters.hdr_rtv_descriptor_handle = m_hdr_rtv_descriptor_handle;
			skybox_parameters.dsv_descriptor_handle = m_depth_dsv_descriptor_handle;
			skybox_parameters.viewport = m_viewport;
			skybox_parameters.scissors_rect = m_scissors_rect;
			skybox_parameters.equirectangular_texture = &m_environment_texture;
			skybox_parameters.camera_gpu_buffer = m_camera_gpu_buffer;
			
			m_skybox_pass.RenderSkybox(&skybox_parameters);
		}

		{
			TonemapPostprocessParameters tonemap_parameters;
			tonemap_parameters.command_queue = command_queue;
			tonemap_parameters.cbv_srv_uav_heap = renderer->GetCbvSrvUavDescriptorHeap();
			tonemap_parameters.scene_color_buffer = m_scene_color_buffer;
			tonemap_parameters.hdr_uav_descriptor_handle = m_hdr_uav_descriptor_handle;
			tonemap_parameters.backbuffer_uav_descriptor_handle = m_backbuffer_uav_descriptor_handle;
			tonemap_parameters.backbuffer_width = GetClientWidth();
			tonemap_parameters.backbuffer_height = GetClientHeight();

			m_tonemap_pass.Render(&tonemap_parameters);
		}

		{
			auto commandListCopyBackBuffer = command_queue->GetCommandList();

			D3D12_BOX sourceRegion;
			sourceRegion.left = 0;
			sourceRegion.top = 0;
			sourceRegion.right = GetClientWidth();
			sourceRegion.bottom = GetClientHeight();
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			CD3DX12_TEXTURE_COPY_LOCATION Dst(current_back_buffer.get());
			CD3DX12_TEXTURE_COPY_LOCATION Src(m_back_buffer.get());

			CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(
				m_back_buffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

			CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
				current_back_buffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);

			commandListCopyBackBuffer->ResourceBarrier(1, &barrier0);
			commandListCopyBackBuffer->ResourceBarrier(1, &barrier1);

			commandListCopyBackBuffer->CopyTextureRegion(&Dst, 0, 0, 0, &Src, &sourceRegion);

			CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
				current_back_buffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
			CD3DX12_RESOURCE_BARRIER barrier3 = CD3DX12_RESOURCE_BARRIER::Transition(
				m_back_buffer.get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			commandListCopyBackBuffer->ResourceBarrier(1, &barrier2);
			commandListCopyBackBuffer->ResourceBarrier(1, &barrier3);

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_scene_color_buffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
			commandListCopyBackBuffer->ResourceBarrier(1, &barrier);

			command_queue->ExecuteCommandList(commandListCopyBackBuffer);
		}

		{
			auto command_list = command_queue->GetCommandList();

			ID3D12DescriptorHeap* ppHeaps[] = { Application::Get().GetRenderer()->GetCbvSrvUavDescriptorHeap()->GetHeapPtr() };
			command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			command_list->OMSetRenderTargets(1, &backbuffer_handle, FALSE, &m_depth_dsv_descriptor_handle.cpu);

			ImguiRenderFrame(command_list);

			command_queue->ExecuteCommandList(command_list);
		}

		// Present
		{
			auto command_list = command_queue->GetCommandList();

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(current_back_buffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			command_list->ResourceBarrier(1, &barrier);

			m_fence_values[current_backbuffer_index] = command_queue->ExecuteCommandList(command_list);

			current_backbuffer_index = m_pWindow->Present();

			command_queue->WaitForFenceValue(m_fence_values[current_backbuffer_index]);
		}

		m_is_first_frame = false;
	}

}
