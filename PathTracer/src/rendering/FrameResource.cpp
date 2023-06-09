#include "FrameResource.h"

namespace RT
{
	FrameResource::FrameResource(ID3D12Device* device, UINT instances, UINT materials)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdListAlloc)));

		passCB = std::make_unique<UploadBuffer<MainPass>>(device, 1, true);
		objCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, instances, false);
		matCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materials, false);
	}

	FrameResource::~FrameResource()
	{
		passCB.reset();
		objCB.reset();
		matCB.reset();
	}
}