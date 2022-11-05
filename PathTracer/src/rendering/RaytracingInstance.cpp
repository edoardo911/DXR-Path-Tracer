#include "RaytracingInstance.h"

namespace RT
{
	void InstanceData::buildVertexBuffer(ID3D12Device* device, Vertex* vertices, UINT sizeInBytes)
	{
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto b = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &b, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer)));

		if(vertices)
		{
			UINT8* data;
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data)));
			memcpy(data, vertices, sizeInBytes);
			vertexBuffer->Unmap(0, nullptr);
		}

		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = sizeInBytes;
	}

	void InstanceData::buildIndexBuffer(ID3D12Device* device, UINT* indices, UINT sizeInBytes)
	{
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto b = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
		ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &b, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer)));

		if(indices)
		{
			UINT8* data;
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data)));
			memcpy(data, indices, sizeInBytes);
			indexBuffer->Unmap(0, nullptr);
		}

		indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		indexBufferView.SizeInBytes = sizeInBytes;
	}

	RaytracingInstance::RaytracingInstance(InstanceData* data): buffers(data) {}
}