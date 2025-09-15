#include "ColorGrading.h"

namespace RT
{
	/*
	* INPUTS:
	* 0 SRV: LUT table
	* 1 UAV: Input/Output
	* 
	* Samplers: point
	*/

	ColorGrading::ColorGrading(ID3D12Device* device, settings_struct* settings): PostProcessing(device, settings, 1.0F)
	{
		mNeedsInput = false;
		mPassCB = std::make_unique<UploadBuffer<PassData>>(device, 1, true);

		init(device, { L"color_grading" });
	}

	void ColorGrading::effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo)
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
		mCommandList[index]->SetComputeRootConstantBufferView(0, mPassCB->resource()->GetGPUVirtualAddress());
		mCommandList[index]->SetComputeRootDescriptorTable(1, getHeapGpu());
		mCommandList[index]->SetComputeRootDescriptorTable(2, gSamplerHeap->GetGPUDescriptorHandleForHeapStart());

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

	void ColorGrading::buildRootSignature(ID3D12Device* device)
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1);

		CD3DX12_DESCRIPTOR_RANGE samplerRange;
		samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(2, ranges, D3D12_SHADER_VISIBILITY_ALL);
		slotRootParameter[2].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	void ColorGrading::resetData()
	{
		PassData data;
		data.invWidth = 1.0F / settings->width;
		data.invHeight = 1.0F / settings->height;
		mPassCB->copyData(0, data);
	}

	void ColorGrading::onResize(ID3D12Device* device, bool ignoreActiveCheck)
	{
		PostProcessing::onResize(device, ignoreActiveCheck);
		resetData();
	}
}