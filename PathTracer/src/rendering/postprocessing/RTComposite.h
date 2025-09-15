#pragma once

#include "PostProcessing.h"

namespace RT
{
	class RTComposite: public PostProcessing
	{
	public:
		RTComposite(ID3D12GraphicsCommandList* cmdList, ID3D12Device* device, settings_struct* settings);

		void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) override;
	private:
		void buildRootSignature(ID3D12Device* device) override;

		ID3D12GraphicsCommandList* cmdList;
	};
}