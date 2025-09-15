#include "ColorAdjust.h"

namespace RT
{
	/*
	* INPUTS:
	* 0 UAV: Back buffer copy
	*/

	ColorAdjust::ColorAdjust(ID3D12Device* device, settings_struct* settings, ColorAdjustConfig config): PostProcessing(device, settings, 1.0F)
	{
		mNeedsInput = false;

		init(device, { L"color_adjust" });

		passCB = std::make_unique<UploadBuffer<ColorAdjustConfig>>(device, 1, true);
		passCB->copyData(0, config);
	}

	void ColorAdjust::effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo)
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)
		};
		mCommandList[index]->ResourceBarrier(2, barriers);
		mCommandList[index]->CopyResource(mOutputBuffer.Get(), backBuffer);
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		mCommandList[index]->ResourceBarrier(1, barriers);

		mCommandList[index]->SetComputeRootSignature(mRootSignature.Get());

		mCommandList[index]->SetPipelineState(mPSOs[0].Get());
		mCommandList[index]->SetComputeRootConstantBufferView(0, passCB->resource()->GetGPUVirtualAddress());
		mCommandList[index]->SetComputeRootDescriptorTable(1, getHeapGpu());

		UINT numGroupsX = (UINT) ceil(settings->width / 32.0F);
		UINT numGroupsY = (UINT) ceil(settings->height / 32.0F);
		mCommandList[index]->Dispatch(numGroupsX, numGroupsY, 1);

		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		mCommandList[index]->ResourceBarrier(2, barriers);
		mCommandList[index]->CopyResource(backBuffer, mOutputBuffer.Get());
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList[index]->ResourceBarrier(2, barriers);
	}

	void ColorAdjust::buildRootSignature(ID3D12Device* device)
	{
		CD3DX12_DESCRIPTOR_RANGE uavRanges[1];
		uavRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameters[2];
		slotRootParameters[0].InitAsConstantBufferView(0);
		slotRootParameters[1].InitAsDescriptorTable(1, uavRanges);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameters);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}
}