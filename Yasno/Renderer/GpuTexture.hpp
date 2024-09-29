#pragma once

#include <string>

#include <d3d12.h>
#include <wil/com.h>

#include <Renderer/DescriptorHeap.hpp>

namespace ysn
{
	// TODO: make it class, GpuTexture, UAV non uav, get rid of scratch buffer, keep if it's srgb
	class Texture
	{
	public:
		DescriptorHandle descriptor_handle;

		std::wstring name = L"Unnamed Texture";
		bool is_srgb = false;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t num_mips = 0;

		wil::com_ptr<ID3D12Resource> gpuTexture;
		wil::com_ptr<ID3D12Resource> textureUploadHeap; // TODO(task): Delete it after texture uploaded!
	};

	// TODO: TextureUsage textureUsage for srgb
	// TODO: Replace with wstring
	// TODO: Unite with LoadHDRTextureFromFile
	Texture LoadTextureFromFile(const std::string& filename,
		wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
		wil::com_ptr<ID3D12Device5> device,
		const ysn::DescriptorHandle& srv_handle);

	Texture LoadHDRTextureFromFile(const std::string& filename,
		wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
		wil::com_ptr<ID3D12Device5> device,
		const ysn::DescriptorHandle& srv_handle);
} // namespace ysn
