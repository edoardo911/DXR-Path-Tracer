#pragma once

#include "../utils/UploadBuffer.h"

namespace RT
{
	struct FrameResource
	{
	public:
		FrameResource(ID3D12Device* device, UINT passNum, UINT materialsNum, std::vector<UINT16> instances = {}, std::vector<std::tuple<UINT, UINT, UINT>> lods = {});
		FrameResource(const FrameResource&) = delete;
		FrameResource& operator=(const FrameResource&) = delete;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mvCmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> dlssCmdListAlloc;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> denoiserCmdListAlloc;

		std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
		std::vector<std::unique_ptr<UploadBuffer<ObjectCB>>> instanceBuffer;
		std::unique_ptr<UploadBuffer<MaterialConstants>> materialCB = nullptr;
		std::unique_ptr<UploadBuffer<ObjectCB>> instanceBufferRT = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> SBTStorage;

		UINT64 fence = 0;
	};
}