#pragma once

#include "../../utils/header.h"

namespace RT
{
	class PostProcessing
	{
	public:
		PostProcessing(ID3D12Device* device, settings_struct* settings, float scale);
		virtual ~PostProcessing() = default;
		PostProcessing(const PostProcessing&) = delete;
		PostProcessing& operator=(const PostProcessing&) = delete;

		virtual void onResize(ID3D12Device* device, bool ignoreActiveCheck = false);

		virtual void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) = 0;

		inline static void begin(int offset = 0)
		{
			ThrowIfFailed(mCmdListAlloc[offset + 0]->Reset());
			ThrowIfFailed(mCommandList[offset + 0]->Reset(mCmdListAlloc[offset + 0].Get(), nullptr));
			ThrowIfFailed(mCmdListAlloc[offset + 1]->Reset());
			ThrowIfFailed(mCommandList[offset + 1]->Reset(mCmdListAlloc[offset + 1].Get(), nullptr));

			ID3D12DescriptorHeap* heaps[] = { gHeap.Get(), gSamplerHeap.Get() };
			mCommandList[offset + 0]->SetDescriptorHeaps(2, heaps);
			mCommandList[offset + 1]->SetDescriptorHeaps(2, heaps);
		}

		inline static void end(int offset = 0)
		{
			ThrowIfFailed(mCommandList[offset + 0]->Close());
			ThrowIfFailed(mCommandList[offset + 1]->Close());
			mDirty = false;
		}

		inline static bool isDirty() { return mDirty; }
		inline static void dirt() { mDirty = true; }
		inline static ID3D12GraphicsCommandList* getCommandList(UINT index) { return mCommandList[index].Get(); }

		inline bool isActive() const { return mActive; }

		inline void enable(ID3D12Device* device, bool ignoreActiveCheck = false)
		{
			if(!mActive || ignoreActiveCheck)
				makeResident(device);
			mActive = true;
		}

		inline void disable(ID3D12Device* device, bool ignoreActiveCheck = false)
		{
			if(mActive || ignoreActiveCheck)
				evict(device);
			mActive = false;
		}

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getHeapCpu() const
		{
			UINT increment = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(gHeap->GetCPUDescriptorHandleForHeapStart(), mIndex * 16, increment);
		}

		inline CD3DX12_GPU_DESCRIPTOR_HANDLE getHeapGpu() const
		{
			UINT increment = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			return CD3DX12_GPU_DESCRIPTOR_HANDLE(gHeap->GetGPUDescriptorHandleForHeapStart(), mIndex * 16, increment);
		}

		inline static void destroyStaticData()
		{
			mCmdListAlloc[0].Reset();
			mCmdListAlloc[1].Reset();
			mCmdListAlloc[2].Reset();
			mCmdListAlloc[3].Reset();
			mCommandList[0].Reset();
			mCommandList[1].Reset();
			mCommandList[2].Reset();
			mCommandList[3].Reset();
		}
	protected:
		void init(ID3D12Device* device, std::vector<std::wstring> shaderNames);

		void buildCommandObjects(ID3D12Device* device);
		void buildResources(ID3D12Device* device);
		void loadShaders(std::vector<std::wstring> shaderNames);
		void evict(ID3D12Device* device);
		void makeResident(ID3D12Device* device);

		virtual void buildRootSignature(ID3D12Device* device) = 0;
		void buildPSOs(ID3D12Device* device);

		static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdListAlloc[4];
		static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList[4];
		static bool mDirty;

		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> gSamplerHeap;
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> gHeap;
		static UINT gEffectIndex;

		UINT mIndex = 0;

		std::vector<Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		Microsoft::WRL::ComPtr<ID3D12Resource> mInputBuffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateBuffer = nullptr;

		ID3D12Device* mDevice = nullptr;

		bool mActive = true;
		bool mNeedsBuffer = false;
		bool mNeedsInput = true;
		bool mNeedsOutput = true;
		bool mScaleInput = false;
		bool mScaleOutput = false;
		bool mScaleBuffer = false;
		bool mInputIsUav = false;
		bool mInterIsUav = true;
		bool mFollowsDLSSSizes = false;
		bool mBufferHighPrecision = false;
		UINT32 mAdditionalSrvSpace = 0;
		settings_struct* settings;
		float mScale = 1.0F;
	};
}