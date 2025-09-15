#pragma once

#include "PostProcessing.h"

namespace RT
{
	class FXAA: public PostProcessing
	{
	public:
		FXAA(ID3D12Device* device, settings_struct* settings);
		~FXAA() = default;

		void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) override;
	private:
		void buildRootSignature(ID3D12Device* device) override;
	};
}