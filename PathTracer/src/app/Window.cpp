#include "Window.h"

#include "../logging/Logger.h"

namespace RT
{
	Window* Window::mWindow = nullptr;

	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return Window::getWindow()->msgProc(hwnd, msg, wParam, lParam);
	}

	Window::Window(HINSTANCE inst): mWindowInst(inst)
	{
		assert(mWindow == nullptr);
		mWindow = this;
	}

	Window::~Window()
	{
		if(md3dDevice)
			flushCommandQueue();
		if(mSwapChain && settings.fullscreen)
			mSwapChain->SetFullscreenState(FALSE, NULL);
		if(feature)
			NVSDK_NGX_D3D12_ReleaseFeature(feature);
		if(settings.dlss)
			NVSDK_NGX_D3D12_Shutdown();
	}

	bool Window::initialize()
	{
		if(!initMainWindow())
			return false;
		Logger::INFO.log("Initialized Window");
		if(!initDirectX12())
			return false;
		Logger::INFO.log("Initialized DirectX12");
		settings.dlssSupported = initDLSS();
		if(!settings.dlssSupported)
			settings.dlss = DLSS_OFF;
		Logger::INFO.log("Initialized DLSS");

		//logging

		if(settings.fullscreen)
		{
			mSwapChain->SetFullscreenState(TRUE, NULL);
			mSwapChain->ResizeTarget(&mFullscreenMode);
		}

		Logger::INFO.log(L"*** Settings ***");
		Logger::INFO.log(L"Width: " + std::to_wstring(settings.width));
		Logger::INFO.log(L"Height: " + std::to_wstring(settings.height));
		Logger::INFO.log(L"DLSS Width: " + std::to_wstring(settings.dlssWidth));
		Logger::INFO.log(L"DLSS Height: " + std::to_wstring(settings.dlssHeight));
		Logger::INFO.log(L"FPS: " + std::to_wstring(settings.fps));
		if(settings.dlss == DLSS_OFF)
			Logger::INFO.log(L"DLSS: Off");
		else if(settings.dlss == DLSS_PERFORMANCE)
			Logger::INFO.log(L"DLSS: Performance");
		else if(settings.dlss == DLSS_QUALITY)
			Logger::INFO.log(L"DLSS: Quality");
		else if(settings.dlss == DLSS_BALANCED)
			Logger::INFO.log(L"DLSS: Balanced");
		else if(settings.dlss == DLSS_ULTRA_PERFORMANCE)
			Logger::INFO.log(L"DLSS: Ultra Performance");
		else if(settings.dlss == DLSS_ULTRA_QUALITY)
			Logger::INFO.log(L"DLSS: Ultra Quality");
		Logger::INFO.log(L"vSync: " + std::wstring(settings.vSync ? L"On" : L"Off"));
		Logger::INFO.log(L"Fullscreeen " + std::wstring(settings.fullscreen ? L"On" : L"Off"));
		Logger::INFO.log(L"DLSS Supported: " + std::wstring(settings.dlssSupported ? L"Yes" : L"No"));
		Logger::INFO.log(L"HDR: " + std::wstring((settings.backBufferFormat == DXGI_FORMAT_R8G8B8A8_UNORM) ? L"No" : L"Yes"));
		Logger::INFO.log(L"Name: " + settings.name);
		return true;
	}

	bool Window::initMainWindow()
	{
		WNDCLASS wc;
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = MainWndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = mWindowInst;
		wc.hIcon = LoadIcon(0, IDI_APPLICATION);
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) GetStockObject(NULL_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = L"MainWnd";

		if(!RegisterClass(&wc))
		{
			MessageBox(0, L"RegisterClass Failed.", 0, 0);
			return false;
		}

		RECT R = { 0, 0, static_cast<LONG>(settings.width), static_cast<LONG>(settings.height) };
		AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
		int width = R.right - R.left;
		int height = R.bottom - R.top;

		mMainWin = CreateWindow(L"MainWnd", settings.name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mWindowInst, 0);
		if(!mMainWin)
		{
			MessageBox(0, L"CreateWindow Failed.", 0, 0);
			return false;
		}

		ShowWindow(mMainWin, SW_SHOW);
		UpdateWindow(mMainWin);
		return true;
	}

	bool Window::initDirectX12()
	{
	#ifdef _DEBUG
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
		HRESULT hardwareResult = D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&md3dDevice));

		if(FAILED(hardwareResult))
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
			ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), featureLevel, IID_PPV_ARGS(&md3dDevice)));
		}

		ThrowIfFailed(mdxgiFactory->EnumAdapterByLuid(md3dDevice->GetAdapterLuid(), IID_PPV_ARGS(&mAdapter)));
		mAdapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 30000000); //30MB

		//fence
		ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

		//get descriptors size
		mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_FEATURE_DATA_D3D12_OPTIONS5 rtOptions = {};
		ThrowIfFailed(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &rtOptions, sizeof(rtOptions)));
		if(rtOptions.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
			throw std::exception("Ray tracing not supported");

		createCommandObjects();
		getDisplayMode();
		if(mFullscreenMode.Width == 0)
		{
			settings.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			getDisplayMode();
			if(mFullscreenMode.Width == 0)
				throw std::exception("No compatible fullscreen modes");
		}
		createSwapChain();
		onResize();
		return true;
	}

	bool Window::initDLSS()
	{
		int updateDriver = 0;
		UINT minDriverMajorI = 0;
		UINT minDriverMinorI = 0;
		UINT dlssSupported = 0;

		NVSDK_NGX_D3D12_Init_with_ProjectID(KEYS::projectKey, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "Alpha", L"", md3dDevice.Get());

		NVSDK_NGX_Result result = NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
		if(result != NVSDK_NGX_Result_Success)
			return false;

		result = params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &updateDriver);
		NVSDK_NGX_Result minDriverMajor = params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverMajorI);
		NVSDK_NGX_Result minDriverMinor = params->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverMinorI);
		if(NVSDK_NGX_SUCCEED(result) && updateDriver && NVSDK_NGX_SUCCEED(minDriverMajor) && NVSDK_NGX_SUCCEED(minDriverMinor))
		{
			if(settings.dlss)
				MessageBox(nullptr, L"It seems that your NVIDIA drivers are out of date. This version of the driver does not support DLSS and it has been disabled.", L"Driver out of date", MB_OK);
			return false;
		}

		result = params->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			return false;
		result = params->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			return false;

		initDLSSFeature();
		return true;
	}

	void Window::initDLSSFeature()
	{
		if(settings.dlss)
		{
			UINT renderWMax, renderHMax, renderWMin, renderHMin;
			float sharpness = 0.0F;

			NVSDK_NGX_PerfQuality_Value val;
			switch(settings.dlss)
			{
			default:
			case DLSS_PERFORMANCE:
				val = NVSDK_NGX_PerfQuality_Value_MaxPerf;
				break;
			case DLSS_QUALITY:
				val = NVSDK_NGX_PerfQuality_Value_MaxQuality;
				break;
			case DLSS_BALANCED:
				val = NVSDK_NGX_PerfQuality_Value_Balanced;
				break;
			case DLSS_ULTRA_PERFORMANCE:
				val = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
				break;
			case DLSS_ULTRA_QUALITY:
				val = NVSDK_NGX_PerfQuality_Value_UltraQuality;
				break;
			}

			NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(params, settings.width, settings.height, val, &settings.dlssWidth, &settings.dlssHeight, &renderWMax, &renderHMax, &renderWMin, &renderHMin, &sharpness);
		
			if(settings.dlssWidth == 0 || settings.dlssHeight == 0)
				throw DLSSException("Unsupported DLSS mode");

			NVSDK_NGX_DLSS_Create_Params featureDesc = {};
			featureDesc.Feature.InWidth = settings.dlssWidth;
			featureDesc.Feature.InHeight = settings.dlssHeight;
			featureDesc.Feature.InTargetWidth = settings.width;
			featureDesc.Feature.InTargetHeight = settings.height;
			featureDesc.Feature.InPerfQualityValue = val;
			featureDesc.InFeatureCreateFlags = (settings.backBufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : NVSDK_NGX_DLSS_Feature_Flags_None) | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
		
			ThrowIfFailed(mDirectCmdListAlloc->Reset());
			ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

			result = NGX_D3D12_CREATE_DLSS_EXT(mCommandList.Get(), 1, 1, &feature, params, &featureDesc);

			ThrowIfFailed(mCommandList->Close());
			ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
			mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
			flushCommandQueue();

			createDLSSResources();
		}
		else
			NVSDK_NGX_D3D12_Shutdown();
	}

	void Window::resetDLSSFeature()
	{
		if(feature)
			NVSDK_NGX_D3D12_ReleaseFeature(feature);
		initDLSSFeature();
	}

	void Window::createDLSSResources()
	{
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		//motion vector buffer
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.DepthOrArraySize = 1;
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		resDesc.Width = settings.dlssWidth;
		resDesc.Height = settings.dlssHeight;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.MipLevels = 1;
		md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mMotionVectorBuffer));

		//depth buffer
		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDepthBuffer));

		//resolved buffer
		resDesc.Format = settings.backBufferFormat;
		resDesc.Width = settings.width;
		resDesc.Height = settings.height;
		md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mResolvedBuffer));
	}

	void Window::createCommandObjects()
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
		ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc)));
		ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(&mCommandList)));
		mCommandList->Close();
	}

	void Window::createSwapChain()
	{
		mSwapChain.Reset();

		DXGI_SWAP_CHAIN_DESC1 sd;
		sd.Width = settings.dlss ? settings.dlssWidth : settings.width;
		sd.Height = settings.dlss ? settings.dlssHeight : settings.height;
		sd.Format = settings.backBufferFormat;
		sd.Stereo = false;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = swapChainBufferCount;
		sd.Scaling = DXGI_SCALING_NONE;
		sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
		ThrowIfFailed(mdxgiFactory->CreateSwapChainForHwnd(mCommandQueue.Get(), mMainWin, &sd, nullptr, nullptr, &swapChain1));
		ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(&mSwapChain)));

		mSwapChain->SetMaximumFrameLatency(swapChainBufferCount);
	}

	int Window::run()
	{
		MSG msg = { 0 };
		double counter = 0;
		resetFPS();
		mTimer.reset();

		while(msg.message != WM_QUIT)
		{
			if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				mTimer.tick();
				if(!mWindowPaused)
				{
					update();
					counter += mTimer.deltaTime();

					if(counter > mFrameTime)
					{
						counter -= mFrameTime;
						calculateFrameStats();
						if(!mFrameInExecution)
							draw();
					}
				}
				else
					Sleep(100);
			}
		}

		return 0;
	}

	LRESULT Window::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch(msg)
		{
		case WM_ACTIVATE:
			if(LOWORD(wParam) == WA_INACTIVE)
			{
				mWindowPaused = true;
				mTimer.stop();

				if(settings.fullscreen)
				{
					toggleFullscreen();
					ShowWindow(mMainWin, SW_SHOWMINIMIZED);
				}
			}
			else
			{
				mWindowPaused = false;
				mTimer.start();
			}
			return 0;
		case WM_SIZE:
			settings.width = LOWORD(lParam);
			settings.height = HIWORD(lParam);

			if(md3dDevice)
			{
				if(wParam == SIZE_MINIMIZED)
				{
					mWindowPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if(wParam == SIZE_MAXIMIZED)
				{
					mWindowPaused = false;
					mMinimized = false;
					mMaximized = true;
					onResize();
				}
				else if(wParam == SIZE_RESTORED)
				{
					if(mMinimized)
					{
						mWindowPaused = false;
						mMinimized = false;
						onResize();
					}
					else if(mMaximized)
					{
						mWindowPaused = false;
						mMaximized = false;
						onResize();
					}
					else if(!mResizing)
						onResize();
				}
			}
			return 0;
		case WM_ENTERSIZEMOVE:
			mWindowPaused = true;
			mResizing = true;
			mTimer.stop();
			return 0;
		case WM_EXITSIZEMOVE:
			mWindowPaused = false;
			mResizing = false;
			mTimer.start();
			onResize();
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_MENUCHAR:
			return MAKELRESULT(0, MNC_CLOSE);
		case WM_GETMINMAXINFO:
			((MINMAXINFO*) lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*) lParam)->ptMinTrackSize.y = 200;
			return 0;
		case WM_LBUTTONDOWN:
			mouse.onButtonDown(MOUSE_LBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_MBUTTONDOWN:
			mouse.onButtonDown(MOUSE_MBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_RBUTTONDOWN:
			mouse.onButtonDown(MOUSE_RBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_LBUTTONUP:
			mouse.onButtonReleased(MOUSE_LBUTTON);
			ReleaseCapture();
			return 0;
		case WM_MBUTTONUP:
			mouse.onButtonReleased(MOUSE_MBUTTON);
			ReleaseCapture();
			return 0;
		case WM_RBUTTONUP:
			mouse.onButtonReleased(MOUSE_RBUTTON);
			ReleaseCapture();
			return 0;
		case WM_MOUSEMOVE:
			mouse.onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			keyboard.onKeyPressed(static_cast<UINT>(wParam));
			return 0;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			keyboard.onKeyReleased(static_cast<UINT>(wParam));
			return 0;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Window::DLSS(ID3D12Resource* outputResource, bool reset)
	{
		NVSDK_NGX_D3D12_DLSS_Eval_Params evalDesc = {};
		evalDesc.Feature.pInColor = outputResource;
		evalDesc.Feature.pInOutput = mResolvedBuffer.Get();
		evalDesc.pInDepth = mDepthBuffer.Get();
		evalDesc.pInMotionVectors = mMotionVectorBuffer.Get();
		evalDesc.InJitterOffsetX = 0;
		evalDesc.InJitterOffsetY = 0;
		evalDesc.InRenderSubrectDimensions = { settings.dlssWidth, settings.dlssHeight };
		evalDesc.InReset = reset ? 1 : 0;
		
		NGX_D3D12_EVALUATE_DLSS_EXT(mCommandList.Get(), feature, params, &evalDesc);
	}

	void Window::calculateFrameStats()
	{
		static int frameCnt = 0;
		static float timeElapsed = 0.0F;

		frameCnt++;
		if((mTimer.totalTime() - timeElapsed) >= 1.0F)
		{
			mFPS = (float) frameCnt;
			//float mspf = 1000.0F / mFPS;

			frameCnt = 0;
			timeElapsed += 1.0F;

			DXGI_QUERY_VIDEO_MEMORY_INFO info;
			ThrowIfFailed(mAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info));
			mbsUsed = info.CurrentUsage / powf(1024, 2);
			percUsedVMem = (float) info.CurrentUsage / info.Budget;

			Logger::INFO.log(L"FPS: " + std::to_wstring(mFPS) + L", " + std::to_wstring(mbsUsed) + L"MB (" + std::to_wstring(percUsedVMem * 100) + L"%)");
		}
	}

	void Window::onResize()
	{
		assert(md3dDevice);
		assert(mSwapChain);
		assert(mDirectCmdListAlloc);

		flushCommandQueue();

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		for(int i = 0; i < swapChainBufferCount; ++i)
			mSwapChainBuffers[i].Reset();

		ThrowIfFailed(mSwapChain->ResizeBuffers(swapChainBufferCount, settings.width, settings.height, settings.backBufferFormat, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
		mCurrBackBuffer = 0;

		for(int i = 0; i < swapChainBufferCount; ++i)
			ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffers[i])));

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsList);

		flushCommandQueue();
	}

	void Window::flushCommandQueue()
	{
		mCurrentFence++;

		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
		if(mFence->GetCompletedValue() < mCurrentFence)
		{
			HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
			if(!eventHandle)
				throw std::exception("Error creating event handle");
			else
			{
				ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
				WaitForSingleObject(eventHandle, INFINITE);
				CloseHandle(eventHandle);
			}
		}
	}

	void Window::getDisplayMode()
	{
		UINT w = GetSystemMetrics(SM_CXSCREEN);
		UINT h = GetSystemMetrics(SM_CYSCREEN);

		if(settings.fullscreen)
		{
			settings.saveSize();
			settings.width = w;
			settings.height = h;
		}

		UINT count = 0;
		IDXGIOutput* output = nullptr;

		if(mAdapter->EnumOutputs(0, &output) == DXGI_ERROR_NOT_FOUND) return;

		output->GetDisplayModeList(settings.backBufferFormat, 0, &count, nullptr);

		std::vector<DXGI_MODE_DESC> modes(count);
		output->GetDisplayModeList(settings.backBufferFormat, 0, &count, &modes[0]);

		for(auto& m:modes)
			if(m.Width == w && m.Height == h && m.RefreshRate.Numerator > mFullscreenMode.RefreshRate.Numerator)
				mFullscreenMode = m;
		output->Release();
	}

	void Window::centerCursor()
	{
		RECT r, f;
		GetWindowRect(mMainWin, &r);
		GetClientRect(mMainWin, &f);
		UINT border = r.bottom - r.top - f.bottom - 8;
		SetCursorPos(r.left + (r.right - r.left) / 2, settings.fullscreen ? (settings.height / 2) : (r.top + border + f.bottom / 2));
	}

	void Window::toggleFullscreen()
	{
		flushCommandQueue();

		if(settings.fullscreen)
		{
			settings.restoreSize();

			DXGI_MODE_DESC mode = {};
			mode.Width = settings.width;
			mode.Height = settings.height;

			mSwapChain->SetFullscreenState(FALSE, NULL);
			mSwapChain->ResizeTarget(&mode);
		}
		else
		{
			settings.saveSize();

			mSwapChain->SetFullscreenState(TRUE, NULL);
			mSwapChain->ResizeTarget(&mFullscreenMode);
		}

		onResize();
		settings.fullscreen = !settings.fullscreen;
	}
}