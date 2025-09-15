#pragma once

#include "Renderer.h"

#include "../raytracing/BottomLevelASGenerator.h"
#include "../raytracing/TopLevelASGenerator.h"
#include "../raytracing/RaytracingPipelineGenerator.h"
#include "../raytracing/ShaderBindingTableGenerator.h"
#include "../raytracing/RootSignatureGenerator.h"

#include "postprocessing/RestirSpatial.h"
#include "postprocessing/RTComposite.h"

namespace RT
{
	class RaytracingRenderer: public Renderer
	{
	public:
		RaytracingRenderer(settings_struct* settings);
		~RaytracingRenderer();

		bool initContext(std::string sceneName) override;

		void updateFrameData() override;
		void draw() override;
		void onResize() override;

		void loadScene(std::string sceneName) override;
	private:
		//MVs
		void loadShadersAndInputLayout() override;
		void buildMVRootSignature();
		void buildMVPSOs();
		void drawEntities(RenderLayer layer, ID3D12GraphicsCommandList* cmdList = nullptr);
		void drawMVAndDepth();

		void createRtvAndDsvDescriptorHeaps() override;
		void allocatePostProcessingResources() override;
		std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> getStaticSamplers();
		void buildFrameResources();

		//denoiser
		bool initDenoiser();
		void createDenoiserPipelines();
		void createDenoiserResources();
		void createCommonTextures();

		//update
		void rebuildTLAS();
		void updateBLAS();
		void updateTLAS();
		void updateMainPassCB();
		void updateObjCB();
		void updateMaterialCB();
		void updateDenoiser();

		void denoise();
		void denoiseAndComposite();

		struct AccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
			Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
			Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc;
		};

		AccelerationStructureBuffers createBottomLevelAS(std::string name, const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vVertexBuffers,
														 const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vIndexBuffers,
														 bool alphaTested, bool allowUpdate, bool tessellated);
		void createTopLevelAS(const std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX, bool, bool>>& instances);
		void createAccelerationStructures();
		void createRayGenSignature(ID3D12RootSignature** pRootSig);
		void createMissSignature(ID3D12RootSignature** pRootSig);
		void createHitSignature(ID3D12RootSignature** pRootSig);
		void createIndirectSignature(ID3D12RootSignature** pRootSig);
		void createEmptySignature(ID3D12RootSignature** pRootSig);
		void createShadowHitSignature(ID3D12RootSignature** pRootSig);
		void createRaytracingPipeline();
		void createShaderBindingTable(bool reload = false);
		void allocateRaytracingResources();

		inline D3D12_CPU_DESCRIPTOR_HANDLE dlssBufferView(UINT backBufferIndex) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBufferCount + backBufferIndex, mRtvDescriptorSize);
		}

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mMVShaders;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mMVPSOs;
		
		//BLAS
		std::unordered_map<std::string, nv_helpers_dx12::BottomLevelASGenerator> mBottomLevelAS;
		std::unordered_map<std::string, AccelerationStructureBuffers> mBlbs;

		//TLAS
		nv_helpers_dx12::TopLevelASGenerator mTopLevelASGenerator;
		AccelerationStructureBuffers mTopLevelASBuffers;
		std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX, bool, bool>> mInstances;

		//RT pipeline
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> mShaders;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mSignatures;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mRtStateObject;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mRtStateObjectProps;
		nv_helpers_dx12::ShaderBindingTableGenerator mSBTHelper;

		Microsoft::WRL::ComPtr<ID3D12Resource> mDiffuse = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSpecular = nullptr;

		//denoiser
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mDenoiserCmdList;

		nrd::Instance* mDenoiser = nullptr;
		nrd::CommonSettings nrdSettings = {};
		std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> mDenoiserRootSignatures;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> mDenoiserPipelines;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mDenoiserResources;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDenoiserResourcesHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDenoiserSamplerHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDenoiserCBV;

		Microsoft::WRL::ComPtr<ID3D12Resource> mDenoisedDiffuse;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDenoisedSpecular;
		Microsoft::WRL::ComPtr<ID3D12Resource> mNormalRoughness;
		Microsoft::WRL::ComPtr<ID3D12Resource> mZDepth;
		Microsoft::WRL::ComPtr<ID3D12Resource> mShadowData;
		Microsoft::WRL::ComPtr<ID3D12Resource> mShadowDenoised;
		Microsoft::WRL::ComPtr<ID3D12Resource> mShadowTranslucency;
		Microsoft::WRL::ComPtr<ID3D12Resource> mMVCopy;
		Microsoft::WRL::ComPtr<ID3D12Resource> mAlbedo;
		Microsoft::WRL::ComPtr<ID3D12Resource> mRF0;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDiffSH1;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSpecSH1;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDiffSH1Denoised;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSpecSH1Denoised;
		Microsoft::WRL::ComPtr<ID3D12Resource> mViewAndRF0;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDiffConfidence;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSpecConfidence;

		DirectX::XMFLOAT2 jitterPrev = { 0.0F, 0.0F };

		std::unique_ptr<RTComposite> mRTComposite;
	};
}