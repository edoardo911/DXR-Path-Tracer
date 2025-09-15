#include "FrameResource.h"

namespace RT
{
	FrameResource::FrameResource(ID3D12Device* device, UINT passNum, UINT materialsNum, std::vector<UINT16> instances, std::vector<std::tuple<UINT, UINT, UINT>> lods)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdListAlloc)));
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mvCmdListAlloc)));
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dlssCmdListAlloc)));

		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&denoiserCmdListAlloc)));

		passCB = std::make_unique<UploadBuffer<PassConstants>>(device, passNum, true);
		for(UINT16 count:instances)
		{
			if(count > 0)
				instanceBuffer.push_back(std::make_unique<UploadBuffer<ObjectCB>>(device, count, false));
		}
		if(materialsNum > 0)
			materialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialsNum, false);

		int i = 0;
		for(UINT16 num:instances)
			i += num;
		if(i > 0)
			instanceBufferRT = std::make_unique<UploadBuffer<ObjectCB>>(device, i, false);
	}
}