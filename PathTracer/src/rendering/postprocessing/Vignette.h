#pragma once

#include "PostProcessing.h"
#include "../../utils/UploadBuffer.h"

namespace RT
{
	class Vignette: public PostProcessing
	{
	public:
		Vignette(ID3D12Device* device, settings_struct* settings);
		~Vignette() = default;

		void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) override;

		void resetData();
	private:
		struct VignetteConfig
		{
			float invWidth;
			float invHeight;
		};

		void buildRootSignature(ID3D12Device* device) override;

		std::unique_ptr<UploadBuffer<VignetteConfig>> passCB;
	};
}