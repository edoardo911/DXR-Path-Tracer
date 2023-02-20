#pragma once

#include "../utils/header.h"

#include "../utils/Timer.h"

#include "../input/Keyboard.h"
#include "../input/Mouse.h"

#include <nvsdk_ngx_helpers.h>

#include <NRD.h>

#include <NRDSettings.h>
#include <NRDDescs.h>

namespace RT
{
	class DLSSException: public std::exception
	{
	public:
		explicit inline DLSSException(const char* message): msg(message) {}
		explicit inline DLSSException(const std::string& message) : msg(message) {}
		inline ~DLSSException() noexcept {}
		const char* what() const noexcept override { return msg.c_str(); }
	private:
		std::string msg;
	};

	class Window
	{
	public:
		inline static Window* getWindow() { return mWindow; }
		inline HINSTANCE windowInst() const { return mWindowInst; }
		inline HWND mainWin() const { return mMainWin; }
		inline float aspectRatio() const { return static_cast<float>(settings.width) / settings.height; }

		virtual ~Window();

		virtual bool initialize();
		int run();

		virtual LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	protected:
		static Window* mWindow;

		Window(HINSTANCE hInstance);
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		virtual void update() = 0;
		virtual void draw() = 0;
		virtual void onResize();

		inline void resetFPS()
		{
			if(settings.fps <= 0)
				mFrameTime = 0.0;
			else
				mFrameTime = 1.0 / settings.fps;
		}

		inline void toggleCursor(bool active) const { ShowCursor(active); }

		void centerCursor();

		bool initMainWindow();
		bool initDirectX12();
		bool initDLSS();
		bool initDenoiser();
		void initDLSSFeature();
		void resetDLSSFeature();

		void getDisplayMode();
		void toggleFullscreen();

		void calculateFrameStats();

		void createDLSSResources();
		void createDenoiserPipelines();
		void createDenoiserResources();
		void DLSS(ID3D12Resource* outputResource, float jitterX = 0.0F, float jitterY = 0.0F, bool reset = false);

		void createCommandObjects();
		void createSwapChain();
		void flushCommandQueue();

		inline ID3D12Resource* currentBackBuffer() const { return mSwapChainBuffers[mCurrBackBuffer].Get(); }

		Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain2> mSwapChain;
		Microsoft::WRL::ComPtr<ID3D12Device5> md3dDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> mAdapter;

		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		UINT64 mCurrentFence = 0;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;

		static const int swapChainBufferCount = 2;
		Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffers[swapChainBufferCount];

		//dlss resources
		Microsoft::WRL::ComPtr<ID3D12Resource> mDepthBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mMotionVectorBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mResolvedBuffer;

		//denoiser
		nrd::Denoiser* mDenoiser;
		std::vector<Microsoft::WRL::ComPtr<ID3D12RootSignature>> mDenoiserRootSignatures;
		std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> mDenoiserPipelines;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mDenoiserResources;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDenoiserCBVHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDenoiserSamplerHeap;

		NVSDK_NGX_Handle* feature = nullptr;
		NVSDK_NGX_Parameter* params = nullptr;

		HINSTANCE mWindowInst = nullptr;

		int mCurrBackBuffer = 0;

		UINT mRtvDescriptorSize = 0;
		UINT mDsvDescriptorSize = 0;
		UINT mCbvSrvUavDescriptorSize = 0;

		HWND mMainWin = nullptr;

		DXGI_MODE_DESC mFullscreenMode = {};

		bool mWindowPaused = false;
		bool mMinimized = false;
		bool mMaximized = false;
		bool mResizing = false;

		bool mFrameInExecution = false;

		Timer mTimer;

		Keyboard keyboard{};
		Mouse mouse{};

		settings_struct settings{};

		float mFPS = 0;
		float percUsedVMem = 0;
		float mbsUsed = 0;

		int phaseCount = 0;
	private:
		double mFrameTime = 0.0;
	};
}