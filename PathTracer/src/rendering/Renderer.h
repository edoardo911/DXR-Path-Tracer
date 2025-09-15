#pragma once

#include "../app/Scene.h"

#include "../utils/header.h"

#include "FrameResource.h"
#include "Camera.h"

#include "../rendering/postprocessing/PostProcessing.h"

#include "../input/Mouse.h"

#define EFFECT_RESTIR_SPATIAL		0
#define EFFECT_FXAA					1
#define EFFECT_COLOR_ADJUST			2
#define EFFECT_COLOR_GRADING		3
#define EFFECT_VIGNETTE				4

namespace RT
{
	class Renderer
	{
		friend class Window;
	public:
		Renderer(settings_struct* settings);
		virtual ~Renderer();
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		bool initDX12(HWND hWnd);
		bool initDLSS();
		virtual bool initContext(std::string sceneName) = 0;

		virtual void update(float dt);
		virtual void updateFrameData() = 0;
		virtual void draw() = 0;
		virtual void onResize() = 0;

		void flushCommandQueue();
		void toggleFullscreen();

		inline Camera* getCamera() const { return mCam; }
		inline ID3D12Device* getDevice() const { return md3dDevice.Get(); }

		inline void queryVideoInfo()
		{
			DXGI_QUERY_VIDEO_MEMORY_INFO info;
			ThrowIfFailed(mAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info));
			mbsUsed = info.CurrentUsage / powf(1024, 2);
			percUsedVMem = ((float) info.CurrentUsage / info.Budget) * 100.0F;
		}

		inline Scene* getScene() const { return mScene.get(); }

		void toggleEffect(int effect, bool active);

		inline bool frameInExecution()
		{
			return mCurrFrameResource && mCurrFrameResource->fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->fence;
		}

		void walk(float dx);
		void strafe(float dx);

		virtual void loadScene(std::string name) = 0;

		inline void toggleFirstPerson() { mFirstPerson = !mFirstPerson; }

		inline void switchCamera(int index)
		{
			if(mScene)
			{
				mScene->switchCamera(index);
				mCam = mScene->getSelectedCamera();
			}
		}
	protected:
		inline void printMemoryInfo()
		{
			Logger::DEBUG.log("Used VRAM: " + std::to_string(mbsUsed) + "MBs, " + std::to_string(percUsedVMem) + "%%");
		}

		inline float aspectRatio() const { return static_cast<float>(settings->width) / settings->height; }

		void getDisplayMode();

		void createCommandObjects();
		void createSwapChain(HWND hWnd);
		virtual void createRtvAndDsvDescriptorHeaps() = 0;
		void buildEffects();
		virtual void allocatePostProcessingResources();
		void buildDefaultFrameResources();

		void loadBlueNoiseTexture();
		void loadLUT();

		virtual void loadShadersAndInputLayout();

		//DLSS
		NVSDK_NGX_PerfQuality_Value queryDLSSModeResolution();
		void initDLSSFeature();
		void resetDLSSFeature();
		void createDLSSResources();
		void initRayReconstructionFeature();
		void resetRayReconstructionFeature();

		void DLSS(ID3D12Resource* outputResource, ID3D12Resource* depthBuffer, DirectX::XMFLOAT2 jitter, bool reset = false);
		void rayReconstruction(ID3D12Resource* diffAlbedo, ID3D12Resource* specAlbedo, ID3D12Resource* normalsAndRough, ID3D12Resource* colorInput, ID3D12Resource* diffDist,
							   ID3D12Resource* specDist, float* viewMatrix, float* projMatrix, DirectX::XMFLOAT2 jitter, bool reset = false);

		void drawEffects(int from, int to, bool first);

		inline ID3D12Resource* currentBackBuffer() const { return mSwapChainBuffer[mCurrBackBuffer].Get(); }
		inline ID3D12Resource* currentDLSSBuffer() const { return mDLSSBuffers[mCurrBackBuffer].Get(); }

		inline D3D12_CPU_DESCRIPTOR_HANDLE backBufferView(UINT backBufferIndex) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), backBufferIndex, mRtvDescriptorSize);
		}

		inline D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, mRtvDescriptorSize);
		}

		inline D3D12_CPU_DESCRIPTOR_HANDLE currentDLSSBufferView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBufferCount * 2 + mCurrBackBuffer, mRtvDescriptorSize);
		}

		Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain2> mSwapChain;
		Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> mAdapter;

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		UINT64 mCurrentFence = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mMvSignature = nullptr;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mMVCommandList;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mDLSSCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		UINT mRtvDescriptorSize = 0;
		UINT mDsvDescriptorSize = 0;
		UINT mCbvSrvUavDescriptorSize = 0;
		UINT mSamplerDescriptorSize = 0;

		DXGI_MODE_DESC mFullscreenMode = {};

		PassConstants mMainPassCB;

		settings_struct* settings;

		static const int swapChainBufferCount = 2;
		HANDLE mFrameWaitable;

		Camera* mCam;
		DirectX::BoundingFrustum mCamFrustum;
		bool mFirstPerson = false;

		std::unique_ptr<Scene> mScene;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

		Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[swapChainBufferCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mMVBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSkyBuffer;
		std::unique_ptr<Texture> mBlueNoiseTex;
		std::unique_ptr<Texture> mLUT;
		int mCurrBackBuffer = 0;

		NVSDK_NGX_Handle* mFeature = nullptr;
		NVSDK_NGX_Parameter* mParams = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mResolvedBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mDLSSBuffers[swapChainBufferCount];
		bool resetDLSS = false;

		std::vector<std::unique_ptr<FrameResource>> frameResources;
		FrameResource* mCurrFrameResource = nullptr;
		int mCurrFrameResourceIndex = 0;

		DirectX::XMFLOAT2 jitter = { 0.0F, 0.0F };
		int phase = 0;

		std::vector<std::unique_ptr<PostProcessing>> mEffects;

		//ray tracing resources
		Microsoft::WRL::ComPtr<ID3D12Resource> mCandidates;
		Microsoft::WRL::ComPtr<ID3D12Resource> mCandidateHistory;

		float mbsUsed = 0.0F;
		float percUsedVMem = 0.0F;
	};
}