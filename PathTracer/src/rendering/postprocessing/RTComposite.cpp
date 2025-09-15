#include "RTComposite.h"

namespace RT
{
	/*
	* INPUTS:
	* 0 SRV: Diffuse
	* 1 SRV: Specular
	* 2 SRV: Sky buffer
	* 3 SRV: Albedo
	* 4 SRV: Shadow data
	* 5 SRV: Fresnel RF0
	* 6 SRV: Diffuse spherical harmonics
	* 7 SRV: Specular spherical harmonics
	* 8 SRV: Normals and roughness
	* 9 SRV: View direction
	* 10 SRV: View z
	* 11 UAV: Output
	*/

	RTComposite::RTComposite(ID3D12GraphicsCommandList* cmdList, ID3D12Device* device, settings_struct* settings): PostProcessing(device, settings, 1.0F), cmdList(cmdList)
	{
		mFollowsDLSSSizes = true;
		mAdditionalSrvSpace = 10;

		init(device, { L"rt_composite" });
	}

	void RTComposite::buildRootSignature(ID3D12Device* device)
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 11, 0, 0, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 11);

		CD3DX12_ROOT_PARAMETER slotRootParameter;
		slotRootParameter.InitAsDescriptorTable(2, ranges, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, &slotRootParameter);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	void RTComposite::effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo)
	{
		auto t = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		cmdList->ResourceBarrier(1, &t);
		cmdList->CopyResource(mInputBuffer.Get(), backBuffer);
		t = CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &t);

		ID3D12DescriptorHeap* heaps[] = { gHeap.Get() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetComputeRootSignature(mRootSignature.Get());

		cmdList->SetPipelineState(mPSOs[0].Get());
		cmdList->SetComputeRootDescriptorTable(0, getHeapGpu());

		UINT numGroupsX = (UINT) ceil(settings->getWidth() / 32.0F);
		UINT numGroupsY = (UINT) ceil(settings->getHeight() / 16.0F);
		cmdList->Dispatch(numGroupsX, numGroupsY, 1);

		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(copyTo, settings->dlss ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmdList->ResourceBarrier(3, barriers);
		cmdList->CopyResource(copyTo, mOutputBuffer.Get());
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(copyTo, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->ResourceBarrier(3, barriers);
	}
}