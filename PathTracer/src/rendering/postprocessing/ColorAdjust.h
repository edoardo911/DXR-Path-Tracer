#pragma once

#include "PostProcessing.h"
#include "../../utils/UploadBuffer.h"

namespace RT
{
	struct ColorAdjustConfig
	{
		float exposure = 1.0F;
		float brightness = 0.0F;
		float contrast = 1.0F;
		float saturation = 1.0F;
		float gamma = 2.2F;
		UINT tonemapping = TONEMAPPING_OFF;
	};

	class ColorAdjust: public PostProcessing
	{
	public:
		ColorAdjust(ID3D12Device* device, settings_struct* settings, ColorAdjustConfig config = {});
		~ColorAdjust() = default;

		void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) override;
	private:
		void buildRootSignature(ID3D12Device* device) override;

		std::unique_ptr<UploadBuffer<ColorAdjustConfig>> passCB;
	};
}