#include "RestirSpatial.h"

namespace RT
{
	/*
	* INPUTS:
	* 0 SRV: Input reservoirs
	* 1 SRV: Depth buffer
	* 2 SRV: Normals and roughness
	* 3 SRV: Fresnel RF0
	* 4 UAV: Spatial reused reservoirs
	*/

	RestirSpatial::RestirSpatial(ID3D12Device* device, settings_struct* settings): PostProcessing(device, settings, 0.0F)
	{
		mAdditionalSrvSpace = 3;

		mCB = std::make_unique<UploadBuffer<PassCB>>(device, 1, true);

		init(device, { L"restir_spatial" });
	}

	void RestirSpatial::buildRootSignature(ID3D12Device* device)
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 4);

		CD3DX12_DESCRIPTOR_RANGE samplerRange;
		samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameters[3];
		slotRootParameters[0].InitAsConstantBufferView(0);
		slotRootParameters[1].InitAsDescriptorTable(2, ranges, D3D12_SHADER_VISIBILITY_ALL);
		slotRootParameters[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameters);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	void RestirSpatial::effect(UINT index, ID3D12Resource* candidates, ID3D12Resource* history)
	{
		CD3DX12_RESOURCE_BARRIER barriers[2] = {
					CD3DX12_RESOURCE_BARRIER::Transition(candidates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ),
					CD3DX12_RESOURCE_BARRIER::Transition(history, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		mCommandList[index]->ResourceBarrier(2, barriers);

		mCommandList[index]->SetComputeRootSignature(mRootSignature.Get());

		mCommandList[index]->SetPipelineState(mPSOs[0].Get());
		mCommandList[index]->SetComputeRootConstantBufferView(0, mCB->resource()->GetGPUVirtualAddress());
		mCommandList[index]->SetComputeRootDescriptorTable(1, getHeapGpu());
		mCommandList[index]->SetComputeRootDescriptorTable(2, gSamplerHeap->GetGPUDescriptorHandleForHeapStart());

		UINT numGroupsX = (UINT) ceil(settings->getWidth() / 16.0F);
		UINT numGroupsY = (UINT) ceil(settings->getHeight() / 8.0F);
		mCommandList[index]->Dispatch(numGroupsX, numGroupsY, 1);

		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(candidates, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(history, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList[index]->ResourceBarrier(2, barriers);
	}

	void RestirSpatial::setData(DirectX::XMFLOAT4X4 invView, DirectX::XMFLOAT4X4 invProj, DirectX::XMFLOAT3 camPos, UINT frameIndex, UINT lightCount, UINT width, UINT height, Light* lights)
	{
		PassCB cb;
		cb.invView = invView;
		cb.invProj = invProj;
		cb.camPos = camPos;
		cb.frameIndex = frameIndex;
		cb.lightCount = lightCount;
		cb.width = width;
		cb.height = height;
		memcpy(&cb.lights[0], lights, sizeof(Light) * lightCount);
		mCB->copyData(0, cb);
	}
}