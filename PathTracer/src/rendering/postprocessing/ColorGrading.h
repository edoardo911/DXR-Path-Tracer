#pragma once

#include "PostProcessing.h"
#include "../../utils/UploadBuffer.h"

namespace RT
{
	class ColorGrading: public PostProcessing
	{
	public:
		ColorGrading(ID3D12Device* device, settings_struct* settings);
		~ColorGrading() = default;

		void effect(UINT index, ID3D12Resource* backBuffer, ID3D12Resource* copyTo = nullptr) override;

		void onResize(ID3D12Device* device, bool ignoreActiveCheck = false) override;
	private:
		struct PassData
		{
			float invWidth;
			float invHeight;
		};

		void buildRootSignature(ID3D12Device* device) override;
		void resetData();

		std::unique_ptr<UploadBuffer<PassData>> mPassCB;
	};
}