#pragma once

#include "../rendering/FrameResource.h"

namespace RT
{
	enum ObjectType
	{
		OBJECT_TYPE_NORMAL = 0
	};

	class InstanceData
	{
	public:
		inline InstanceData(std::string name): name(name) {}
		~InstanceData() = default;

		void buildVertexBuffer(ID3D12Device* device, Vertex* vertices, UINT sizeInBytes);
		void buildIndexBuffer(ID3D12Device* device, UINT* indices, UINT sizeInBytes);

		UINT vertexCount = 0;
		UINT indexCount = 0;

		std::string name;
		bool alphaTested = false;

		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
	};

	class RaytracingInstance
	{
	public:
		RaytracingInstance(InstanceData* data);
		RaytracingInstance(const RaytracingInstance& rhs) = delete;
		RaytracingInstance& operator=(const RaytracingInstance& rhs) = delete;

		DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX texTransform = DirectX::XMMatrixIdentity();
		INT32 texOffset = -1;
		INT32 normOffset = -1;
		UINT matOffset = 0;
		UINT objCBOffset = 0;
		int numFramesDirty = NUM_FRAME_RESOURCES;

		InstanceData* buffers = nullptr;
		ObjectType type = OBJECT_TYPE_NORMAL;
	};
}