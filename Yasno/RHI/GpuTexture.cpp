#include "GpuTexture.hpp"

#include <d3dx12.h>
#include <stb_image.h>

#include <System/Helpers.hpp>

namespace ysn
{
	Texture LoadTextureFromFile(const std::string& filename,
								wil::com_ptr<ID3D12GraphicsCommandList2> CommandList,
								wil::com_ptr<ID3D12Device5> device,
								const ysn::DescriptorHandle& srv_handle)
	{
		Texture result;

		// TODO(task): Make it dynamic
		// The number of bytes used to represent a pixel in the texture.
		int texturePixelSize = 4 * 1;

		int width, height, texChannels;
		unsigned char* data = stbi_load(filename.c_str(), &width, &height, &texChannels, STBI_rgb_alpha);

		if (data != nullptr)
		{
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			const auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			const auto heapPropertiesUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			ThrowIfFailed(device->CreateCommittedResource(
				&heapPropertiesDefault,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&result.gpuTexture)));

		#ifndef YSN_RELEASE
			std::wstring name(filename.begin(), filename.end());
			result.gpuTexture->SetName(name.c_str());
		#endif

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(result.gpuTexture.get(), 0, 1);

			const auto uploadBufferSizeDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

			// Create the GPU upload buffer.
			ThrowIfFailed(device->CreateCommittedResource(
				&heapPropertiesUpload,
				D3D12_HEAP_FLAG_NONE,
				&uploadBufferSizeDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&result.textureUploadHeap)));

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = data;
			textureData.RowPitch = width * texturePixelSize;
			textureData.SlicePitch = textureData.RowPitch * height;

			const CD3DX12_RESOURCE_BARRIER barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				result.gpuTexture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			UpdateSubresources(CommandList.get(), result.gpuTexture.get(), result.textureUploadHeap.get(), 0, 0, 1, &textureData);

			// CommandList->ResourceBarrier(1, &barrierDesc);

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			device->CreateShaderResourceView(result.gpuTexture.get(), &srvDesc, srv_handle.cpu);

			result.descriptor_handle = srv_handle;

			stbi_image_free(data);
		}

		return result;
	}

	Texture LoadHDRTextureFromFile(const std::string& filename,
								   wil::com_ptr<ID3D12GraphicsCommandList2> CommandList,
								   wil::com_ptr<ID3D12Device5> device,
								   const ysn::DescriptorHandle& srv_handle)
	{
		Texture result;

		// TODO(task): Make it dynamic
		// The number of bytes used to represent a pixel in the texture.
		int texturePixelSize = 4 * 4;

		int width, height, texChannels;
		float* data = stbi_loadf(filename.c_str(), &width, &height, &texChannels, STBI_rgb_alpha);

		if (data != nullptr)
		{
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // TODO: Sheesh its too fat!
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			const auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			const auto heapPropertiesUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			ThrowIfFailed(device->CreateCommittedResource(
				&heapPropertiesDefault,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&result.gpuTexture)));

		#ifndef YSN_RELEASE
			std::wstring name(filename.begin(), filename.end());
			result.gpuTexture->SetName(name.c_str());
		#endif

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(result.gpuTexture.get(), 0, 1);

			const auto uploadBufferSizeDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

			// Create the GPU upload buffer.
			ThrowIfFailed(device->CreateCommittedResource(
				&heapPropertiesUpload,
				D3D12_HEAP_FLAG_NONE,
				&uploadBufferSizeDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&result.textureUploadHeap)));

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = data;
			textureData.RowPitch = width * texturePixelSize;
			textureData.SlicePitch = textureData.RowPitch * height;

			const CD3DX12_RESOURCE_BARRIER barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
				result.gpuTexture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			UpdateSubresources(CommandList.get(), result.gpuTexture.get(), result.textureUploadHeap.get(), 0, 0, 1, &textureData);

			// CommandList->ResourceBarrier(1, &barrierDesc);

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			device->CreateShaderResourceView(result.gpuTexture.get(), &srvDesc, srv_handle.cpu);

			result.descriptor_handle = srv_handle;

			stbi_image_free(data);
		}

		return result;
	}
}
