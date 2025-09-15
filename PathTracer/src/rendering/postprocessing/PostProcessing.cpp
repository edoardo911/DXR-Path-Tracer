#include "PostProcessing.h"

#define EFFECT_COUNT 5

namespace RT
{
	bool PostProcessing::mDirty = true;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> PostProcessing::mCmdListAlloc[4];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> PostProcessing::mCommandList[4];
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> PostProcessing::gSamplerHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> PostProcessing::gHeap = nullptr;
	UINT PostProcessing::gEffectIndex = 0;

	PostProcessing::PostProcessing(ID3D12Device* device, settings_struct* settings, float scale): settings(settings), mScale(scale)
	{
		if(!mDevice)
			mDevice = device;
		mIndex = gEffectIndex++;
	}

	void PostProcessing::init(ID3D12Device* device, std::vector<std::wstring> shaderNames)
	{
		buildCommandObjects(device);
		buildResources(device);
		loadShaders(shaderNames);
		buildRootSignature(device);
		buildPSOs(device);
	}

	void PostProcessing::buildCommandObjects(ID3D12Device* device)
	{
		if(!mCmdListAlloc[0])
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdListAlloc[0])));
		if(!mCmdListAlloc[1])
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdListAlloc[1])));
		if(!mCmdListAlloc[2])
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdListAlloc[2])));
		if(!mCmdListAlloc[3])
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdListAlloc[3])));
		if(!mCommandList[0])
		{
			ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdListAlloc[0].Get(), nullptr, IID_PPV_ARGS(&mCommandList[0])));
			mCommandList[0]->Close();
		}
		if(!mCommandList[1])
		{
			ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdListAlloc[1].Get(), nullptr, IID_PPV_ARGS(&mCommandList[1])));
			mCommandList[1]->Close();
		}
		if(!mCommandList[2])
		{
			ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdListAlloc[0].Get(), nullptr, IID_PPV_ARGS(&mCommandList[2])));
			mCommandList[2]->Close();
		}
		if(!mCommandList[3])
		{
			ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdListAlloc[1].Get(), nullptr, IID_PPV_ARGS(&mCommandList[3])));
			mCommandList[3]->Close();
		}
	}

	void PostProcessing::buildResources(ID3D12Device* device)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = 32 * EFFECT_COUNT;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;
		if(!gHeap)
			ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gHeap)));

		if(!gSamplerHeap)
		{
			heapDesc.NumDescriptors = 2;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&gSamplerHeap)));

			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(gSamplerHeap->GetCPUDescriptorHandleForHeapStart());

			D3D12_SAMPLER_DESC sampler;
			sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.MipLODBias = 0;
			sampler.MinLOD = 0;
			sampler.MaxLOD = 0;
			sampler.MaxAnisotropy = 1;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			device->CreateSampler(&sampler, handle);

			UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			handle.Offset(1, increment);

			sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			device->CreateSampler(&sampler, handle);
		}

		onResize(device);
	}

	void PostProcessing::loadShaders(std::vector<std::wstring> shaderNames)
	{
		for(auto& name:shaderNames)
			mShaders.push_back(LoadBinary(L"res/shaders/bin/" + name + L".cso"));
	}

	void PostProcessing::buildPSOs(ID3D12Device* device)
	{
		mPSOs.clear();
		mPSOs.resize(mShaders.size());

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignature.Get();
		for(int i = 0; i < mShaders.size(); ++i)
		{
			psoDesc.CS = {
				reinterpret_cast<BYTE*>(mShaders[i]->GetBufferPointer()),
				mShaders[i]->GetBufferSize()
			};
			ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[i])));
		}
	}

	void PostProcessing::onResize(ID3D12Device* device, bool ignoreActiveCheck)
	{
		UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		UINT w = mFollowsDLSSSizes ? settings->getWidth() : settings->width;
		UINT h = mFollowsDLSSSizes ? settings->getHeight() : settings->height;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.DepthOrArraySize = 1;
		resDesc.Format = settings->backBufferFormat;
		resDesc.Width = static_cast<UINT64>(w * (mScaleInput ? mScale : 1.0F));
		resDesc.Height = static_cast<UINT>(h * (mScaleInput ? mScale : 1.0F));
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.MipLevels = 1;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = mInputIsUav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
		if(mScale > 0.0F && mNeedsInput)
			ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mInputBuffer)));

		resDesc.Width = static_cast<UINT64>(w * (mScaleOutput ? mScale : 1.0F));
		resDesc.Height = static_cast<UINT>(h * (mScaleOutput ? mScale : 1.0F));
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if(mScale > 0.0F && mNeedsOutput)
			ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mOutputBuffer)));

		if(mNeedsBuffer)
		{
			resDesc.Width = static_cast<UINT64>(w * (mScaleBuffer ? mScale : 1.0F));
			resDesc.Height = static_cast<UINT>(h * (mScaleBuffer ? mScale : 1.0F));
			if(mBufferHighPrecision)
				resDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			if(mScale > 0.0F)
				ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mIntermediateBuffer)));
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = getHeapCpu();

		if(mInputIsUav)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			if(mScale > 0.0F && mNeedsInput)
				device->CreateUnorderedAccessView(mInputBuffer.Get(), nullptr, &uavDesc, handle);
		}
		else
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Format = settings->backBufferFormat;
			if(mScale > 0.0F && mNeedsInput)
				device->CreateShaderResourceView(mInputBuffer.Get(), &srvDesc, handle);
		}

		handle.Offset(mAdditionalSrvSpace + 1, increment);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		if(mScale > 0.0F && mNeedsOutput)
			device->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, handle);

		if(mNeedsBuffer)
		{
			handle.Offset(1, increment);
			if(mInterIsUav)
				device->CreateUnorderedAccessView(mIntermediateBuffer.Get(), nullptr, &uavDesc, handle);
			else
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Format = mBufferHighPrecision ? DXGI_FORMAT_R32G32B32A32_FLOAT : settings->backBufferFormat;
				if(mScale > 0.0F)
					device->CreateShaderResourceView(mIntermediateBuffer.Get(), &srvDesc, handle);
			}
		}

		if(!mActive)
			disable(device, ignoreActiveCheck);
		mDirty = true;
	}

	void PostProcessing::makeResident(ID3D12Device* device)
	{
		std::vector<ID3D12Pageable*> res;
		if(mNeedsInput)
			res.push_back(mInputBuffer.Get());
		if(mNeedsOutput)
			res.push_back(mOutputBuffer.Get());
		if(mNeedsBuffer)
			res.push_back(mIntermediateBuffer.Get());
		if(mScale > 0.0F)
			device->MakeResident((UINT) res.size(), res.data());
	}

	void PostProcessing::evict(ID3D12Device* device)
	{
		std::vector<ID3D12Pageable*> res;
		if(mNeedsInput)
			res.push_back(mInputBuffer.Get());
		if(mNeedsOutput)
			res.push_back(mOutputBuffer.Get());
		if(mNeedsBuffer)
			res.push_back(mIntermediateBuffer.Get());
		if(mScale > 0.0F)
			device->Evict((UINT) res.size(), res.data());
	}
}