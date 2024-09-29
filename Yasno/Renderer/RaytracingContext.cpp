#include "RaytracingContext.hpp"

#include <Renderer/nv_helpers_dx12/TopLevelASGenerator.h>
#include <Renderer/nv_helpers_dx12/BottomLevelASGenerator.h>
#include <Renderer/DXRHelper.h>
#include <System/GltfLoader.hpp>

// TODO(postrtx): remove this
ID3D12Resource* CreateBuffer(ID3D12Device* m_device, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Alignment = 0;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Flags = flags;
	bufDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufDesc.Height = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.SampleDesc.Quality = 0;
	bufDesc.Width = size;

	ID3D12Resource* pBuffer;
	ThrowIfFailed(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer)));
	return pBuffer;
}

// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
ysn::AccelerationStructureBuffers ysn::RaytracingContext::CreateBottomLevelAS(wil::com_ptr<ID3D12Device5> device,
																			  wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
																			  std::vector<BLASVertexInput> vertex_buffers) // sizeof(Vertex)
{
	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

	// Adding all vertex buffers and not transforming their position.
	for (const auto& buffer : vertex_buffers)
	{
		bottomLevelAS.AddVertexBuffer(
			buffer.vertex_buffer.get(),
			buffer.vertex_offset_in_bytes,
			buffer.vertex_count,
			buffer.vertex_stride,
			buffer.index_buffer.get(),
			buffer.index_offset_in_bytes,
			buffer.index_count,
			0,
			0);
	}

	// The AS build requires some scratch space to store temporary information.
	// The amount of scratch memory is dependent on the scene complexity.
	UINT64 scratch_size_in_bytes = 0;

	// The final AS also needs to be stored in addition to the existing vertex
	// buffers. It size is also dependent on the scene complexity.
	UINT64 result_size_in_bytes = 0;

	bottomLevelAS.ComputeASBufferSizes(device.get(), false, &scratch_size_in_bytes, &result_size_in_bytes);

	// Once the sizes are obtained, the application is responsible for allocating
	// the necessary buffers. Since the entire generation will be done on the GPU,
	// we can directly allocate those on the default heap
	AccelerationStructureBuffers buffers;
	buffers.scratch = CreateBuffer(device.get(), scratch_size_in_bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);

	buffers.result = CreateBuffer(
		device.get(),
		result_size_in_bytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nv_helpers_dx12::kDefaultHeapProps);

	// Build the acceleration structure. Note that this call integrates a barrier
	// on the generated AS, so that it can be used to compute a top-level AS right
	// after this method.
	bottomLevelAS.Generate(command_list.get(), buffers.scratch.get(), buffers.result.get(), false, nullptr);

	return buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
// Instances are pair of bottom level AS and matrix of the instance
void ysn::RaytracingContext::CreateTopLevelAS(
	wil::com_ptr<ID3D12Device5> device,
	wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
	const std::vector<std::pair<wil::com_ptr<ID3D12Resource>, DirectX::XMMATRIX>>& input_instances)
{
	// Gather all the instances into the builder helper
	for (size_t i = 0; i < input_instances.size(); i++)
	{
		tlas_generator.AddInstance(input_instances[i].first.get(), input_instances[i].second, static_cast<uint32_t>(i), static_cast<uint32_t>(0));
	}

	// As for the bottom-level AS, the building the AS requires some scratch space
	// to store temporary data in addition to the actual AS. In the case of the
	// top-level AS, the instance descriptors also need to be stored in GPU
	// memory. This call outputs the memory requirements for each (scratch,
	// results, instance descriptors) so that the application can allocate the
	// corresponding memory
	UINT64 scratchSize, resultSize, instanceDescsSize;

	tlas_generator.ComputeASBufferSizes(device.get(), true, &scratchSize, &resultSize, &instanceDescsSize);

	// Create the scratch and result buffers. Since the build is all done on GPU,
	// those can be allocated on the default heap
	tlas_buffers.scratch = CreateBuffer(
		device.get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);

	tlas_buffers.result = CreateBuffer(
		device.get(),
		resultSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nv_helpers_dx12::kDefaultHeapProps);

	// The buffer describing the instances: ID, shader binding information,
	// matrices ... Those will be copied into the buffer by the helper through
	// mapping, so the buffer has to be allocated on the upload heap.
	tlas_buffers.instance_desc = CreateBuffer(
		device.get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

	// After all the buffers are allocated, or if only an update is required, we
	// can build the acceleration structure. Note that in the case of the update
	// we also pass the existing AS as the 'previous' AS, so that it can be
	// refitted in place.
	tlas_generator.Generate(command_list.get(), tlas_buffers.scratch.get(), tlas_buffers.result.get(), tlas_buffers.instance_desc.get());
}

void ysn::RaytracingContext::CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer)
{
	tlas_buffers.tlas_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.RaytracingAccelerationStructure.Location = tlas_buffers.result->GetGPUVirtualAddress();

	// Write the acceleration structure view in the heap
	renderer->GetDevice()->CreateShaderResourceView(nullptr, &srv_desc, tlas_buffers.tlas_srv.cpu);
}

// Combine the BLAS and TLAS builds to construct the entire acceleration structure required to raytrace the scene
void ysn::RaytracingContext::CreateAccelerationStructures(wil::com_ptr<ID3D12Device5> device,
														  wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
														  ysn::ModelRenderContext* gltf_model_context)
{
	std::vector<BLASVertexInput> vertex_buffers;

	// Vertex buffer
	// Amout of vertices
	// Sizeof vertex
	for (const auto& mesh : gltf_model_context->Meshes)
	{
		for (const auto& primitive : mesh.primitives)
		{
			BLASVertexInput blas_input;

			blas_input.vertex_buffer = primitive.vertex_buffer;
			blas_input.vertex_offset_in_bytes = primitive.vertex_offset_in_bytes;
			blas_input.vertex_count = primitive.vertex_count;
			blas_input.vertex_stride = primitive.vertex_stride;
			blas_input.index_buffer = primitive.index_buffer;
			blas_input.index_offset_in_bytes = primitive.index_offset_in_bytes;
			blas_input.index_count = primitive.index_count;

			vertex_buffers.push_back(blas_input);
		}
	}

	// Build the bottom AS from the Triangle vertex buffer
	AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS(device, command_list, vertex_buffers);

	// Just one instance for now
	instances = { { bottomLevelBuffers.result, DirectX::XMMatrixIdentity() } };
	CreateTopLevelAS(device, command_list, instances);

	// Store the AS buffers. The rest of the buffers will be released once we exit
	// the function
	blas_res = bottomLevelBuffers.result;

#ifndef YSN_RELEASE
	blas_res->SetName(L"BLAS");
	tlas_buffers.result->SetName(L"TLAS");
#endif
}
