#pragma once

#include "PostProcessing.h"

#include "../../utils/UploadBuffer.h"

namespace RT
{
	class RestirSpatial: public PostProcessing
	{
	public:
		RestirSpatial(ID3D12Device* device, settings_struct* settings);

		void effect(UINT index, ID3D12Resource* candidates, ID3D12Resource* history = nullptr) override;

		void setData(DirectX::XMFLOAT4X4 invView, DirectX::XMFLOAT4X4 invProj, DirectX::XMFLOAT3 camPos, UINT frameIndex, UINT lightCount, UINT width, UINT height, Light* lights);
	private:
		struct PassCB
		{
			DirectX::XMFLOAT4X4 invView;
			DirectX::XMFLOAT4X4 invProj;
			DirectX::XMFLOAT3 camPos;
			float pad;
			UINT frameIndex;
			UINT lightCount;
			UINT width;
			UINT height;
			Light lights[MAX_LIGHTS];
		};

		void buildRootSignature(ID3D12Device* device) override;

		std::unique_ptr<UploadBuffer<PassCB>> mCB;
	};
}