#pragma once
#include "D3DUtil.h"

namespace RT
{
	template<typename T>
	class UploadBuffer
	{
	public:
		inline UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer): elementCount(elementCount), mIsConstantBuffer(isConstantBuffer)
		{
			mElementByteSize = sizeof(T);

			if(isConstantBuffer)
				mElementByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(T));
			auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto rd = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
			ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mUploadBuffer)));
			ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
		}

		UploadBuffer(const UploadBuffer&) = delete;
		UploadBuffer& operator=(const UploadBuffer&) = delete;

		inline ~UploadBuffer()
		{
			if(mUploadBuffer)
				mUploadBuffer->Unmap(0, nullptr);
			mMappedData = nullptr;
		}

		inline ID3D12Resource* resource() const { return mUploadBuffer.Get(); }

		inline void copyData(int elementIndex, const T& data)
		{
			memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
		}

		UINT elementCount = -1;
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
		BYTE* mMappedData = nullptr;
		UINT mElementByteSize = 0;
		bool mIsConstantBuffer = false;
	};
};