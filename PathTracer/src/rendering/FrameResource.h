#pragma once

#include "../utils/D3DUtil.h"
#include "../utils/UploadBuffer.h"

namespace RT
{
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uvs;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT3 tangent;
	};

	struct MainPass
	{
		DirectX::XMFLOAT4X4 invView;
		DirectX::XMFLOAT4X4 invProj;
		UINT32 frameIndex = 1;
	};

	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 world;
		INT32 diffuseIndex;
		INT32 normalIndex;
		UINT32 materialIndex;
	};

	#define FLAG_MIRROR		0x00000001
	#define FLAG_GLASS		0x00000002

	struct MaterialConstants
	{
		DirectX::XMFLOAT4 diffuseAlbedo;
		DirectX::XMFLOAT3 fresnelR0;
		float fresnelPower = 0.0F;
		float roughness = 0.1F;
		float metallic = 0.0F;
		float refractionIndex = 1.0F;
		UINT32 flags = 0x00000000;
	};

	struct FrameResource
	{
        FrameResource(ID3D12Device* device, UINT instances, UINT materials);
        FrameResource(const FrameResource& rhs) = delete;
        FrameResource& operator=(const FrameResource& rhs) = delete;
        ~FrameResource();

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc;
        Microsoft::WRL::ComPtr<ID3D12Resource> SBTStorage;

		std::unique_ptr<UploadBuffer<MainPass>> passCB;
		std::unique_ptr<UploadBuffer<ObjectConstants>> objCB;
		std::unique_ptr<UploadBuffer<MaterialConstants>> matCB;

        UINT64 fence = 0;
	};
}