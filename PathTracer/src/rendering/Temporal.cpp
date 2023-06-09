#include "Temporal.h"

namespace RT
{
	Temporal::Temporal(ID3D12Device* device, std::wstring shader, const D3D12_STATIC_SAMPLER_DESC* samplers)
	{
		loadShader(device, shader);
		buildRootSignature(device, samplers);
		buildPSO(device);
	}

	void Temporal::loadShader(ID3D12Device* device, std::wstring fileName)
	{
		mShader = LoadBinary(fileName);

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 5;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap)));
	}

	void Temporal::buildRootSignature(ID3D12Device* device, const D3D12_STATIC_SAMPLER_DESC* samplers)
	{
		CD3DX12_DESCRIPTOR_RANGE inputRange;
		inputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, 0);
		CD3DX12_DESCRIPTOR_RANGE outputRange;
		outputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 4);

		CD3DX12_ROOT_PARAMETER parameter;

		D3D12_DESCRIPTOR_RANGE ranges[2] = { inputRange, outputRange };
		parameter.InitAsDescriptorTable(2, ranges, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_ROOT_SIGNATURE_DESC bloomRootSigDesc(1, &parameter, 2, samplers);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&bloomRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
	}

	void Temporal::buildPSO(ID3D12Device* device)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = mRootSignature.Get();
		desc.CS = {
			reinterpret_cast<BYTE*>(mShader->GetBufferPointer()),
			mShader->GetBufferSize()
		};
		ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&mPSO)));
	}

	void Temporal::dispatch(ID3D12GraphicsCommandList* cmdList, UINT32 width, UINT32 height)
	{
		ID3D12DescriptorHeap* heaps[] = { mHeap.Get() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetComputeRootSignature(mRootSignature.Get());

		cmdList->SetPipelineState(mPSO.Get());
		cmdList->SetComputeRootDescriptorTable(0, mHeap->GetGPUDescriptorHandleForHeapStart());

		UINT w = (UINT) ceilf(width / 16.0F);
		UINT h = (UINT) ceilf(height / 16.0F);
		cmdList->Dispatch(w, h, 1);
	}
}