#pragma once

#include "../utils/header.h"

namespace RT
{
	class Temporal
	{
	public:
		Temporal(ID3D12Device* device, std::wstring shader);
		~Temporal() = default;

		void dispatch(ID3D12GraphicsCommandList* cmdList, UINT32 width, UINT32 height);
		inline D3D12_CPU_DESCRIPTOR_HANDLE getHeapStart() { return mHeap->GetCPUDescriptorHandleForHeapStart(); }
	private:
		void loadShader(ID3D12Device* device, std::wstring fileName);
		void buildRootSignature(ID3D12Device* device);
		void buildPSO(ID3D12Device* device);

		Microsoft::WRL::ComPtr<ID3DBlob> mShader;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
	};
}