#include "Renderer.h"

#include "postprocessing/RestirSpatial.h"
#include "postprocessing/FXAA.h"
#include "postprocessing/ColorAdjust.h"
#include "postprocessing/ColorGrading.h"
#include "postprocessing/Vignette.h"

#include "../utils/TextureLoader.h"

using namespace DirectX;

namespace RT
{
	Renderer::Renderer(settings_struct* settings): settings(settings) {}

	Renderer::~Renderer()
	{
		if(md3dDevice)
			flushCommandQueue();
		if(mSwapChain && settings->fullscreen)
			mSwapChain->SetFullscreenState(FALSE, NULL);
		PostProcessing::destroyStaticData();
		NVSDK_NGX_D3D12_ReleaseFeature(mFeature);
		NVSDK_NGX_D3D12_Shutdown1(md3dDevice.Get());
		md3dDevice = nullptr;
	}

	bool Renderer::initDX12(HWND hWnd)
	{
		Logger::INFO.log("Initializing DirectX12...");

	#ifndef UGE_DIST
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
		HRESULT hardwareResult = D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&md3dDevice));
		if(FAILED(hardwareResult))
		{
			Logger::WARN.log("Couldn't find an hardware device, using warp adapter");
			Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
			ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(), featureLevel, IID_PPV_ARGS(&md3dDevice)));
		}

		ThrowIfFailed(mdxgiFactory->EnumAdapterByLuid(md3dDevice->GetAdapterLuid(), IID_PPV_ARGS(&mAdapter)));
		mAdapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, 130000000); //130MB

		//fence
		ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

		//get descriptor size
		mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mSamplerDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		//ray tracing check
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 rtOptions = {};
		ThrowIfFailed(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &rtOptions, sizeof(rtOptions)));
		if(rtOptions.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			throw RaytracingException("Ray tracing is not supported by the GPU");
		}

		createCommandObjects();
		getDisplayMode();
		if(mFullscreenMode.Width == 0)
		{
			Logger::WARN.log("The GPU might not be compatible with HDR");
			settings->backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			getDisplayMode();
			if(mFullscreenMode.Width == 0)
				throw Win32Exception("HDR is not supported by the system");
		}

		createSwapChain(hWnd);
		createRtvAndDsvDescriptorHeaps();

		Logger::INFO.log("Successfully intialized DirectX12");
		return true;
	}

	void Renderer::createCommandObjects()
	{
		Logger::INFO.log("Creating command objects...");

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
		ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc)));

		ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(&mCommandList)));
		ThrowIfFailed(mCommandList->Close());
		ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(&mMVCommandList)));
		ThrowIfFailed(mMVCommandList->Close());

		ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(&mDLSSCommandList)));
		ThrowIfFailed(mDLSSCommandList->Close());
	}

	void Renderer::createSwapChain(HWND hWnd)
	{
		Logger::INFO.log("Creating swap chain...");
		mSwapChain.Reset();

		DXGI_SWAP_CHAIN_DESC1 sd;
		sd.Width = settings->width;
		sd.Height = settings->height;
		sd.Format = settings->backBufferFormat;
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
		ThrowIfFailed(mdxgiFactory->CreateSwapChainForHwnd(mCommandQueue.Get(), hWnd, &sd, nullptr, nullptr, &swapChain1));
		ThrowIfFailed(swapChain1->QueryInterface(IID_PPV_ARGS(&mSwapChain)));

		mSwapChain->SetMaximumFrameLatency(swapChainBufferCount);
		mFrameWaitable = mSwapChain->GetFrameLatencyWaitableObject();
	}

	void Renderer::flushCommandQueue()
	{
		mCurrentFence++;

		ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
		if(mFence->GetCompletedValue() < mCurrentFence)
		{
			HANDLE eventHandle = CreateEventEx(nullptr, 0, 0, EVENT_ALL_ACCESS);
			if(!eventHandle)
				throw Win32Exception("Error creating event handle");
			else
			{
				ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
				WaitForSingleObject(eventHandle, INFINITE);
				CloseHandle(eventHandle);
			}
		}
	}

	void Renderer::getDisplayMode()
	{
		UINT w = GetSystemMetrics(SM_CXSCREEN);
		UINT h = GetSystemMetrics(SM_CYSCREEN);

		UINT count = 0;
		IDXGIOutput* output = nullptr;

		if(mAdapter->EnumOutputs(0, &output) == DXGI_ERROR_NOT_FOUND) return;

		DXGI_ADAPTER_DESC desc;
		mAdapter->GetDesc(&desc);
		settings->gpuInfo = desc.Description;
		settings->vramGB = desc.DedicatedVideoMemory / powf(1024, 3);
		Logger::INFO.log(L"Found GPU \"" + settings->gpuInfo + L"\" with " + std::to_wstring(settings->vramGB) + L"GB VRAM");

		output->GetDisplayModeList(settings->backBufferFormat, 0, &count, nullptr);

		std::vector<DXGI_MODE_DESC> modeList(count);
		output->GetDisplayModeList(settings->backBufferFormat, 0, &count, &modeList[0]);

		for(auto& m : modeList)
			if(m.Width == w && m.Height == h && m.RefreshRate.Numerator > mFullscreenMode.RefreshRate.Numerator)
				mFullscreenMode = m;
		output->Release();
	}

	void Renderer::toggleFullscreen()
	{
		flushCommandQueue();

		if(settings->fullscreen)
		{
			settings->load();

			DXGI_MODE_DESC mode = {};
			mode.Width = settings->width;
			mode.Height = settings->height;

			mSwapChain->SetFullscreenState(FALSE, NULL);
			mSwapChain->ResizeTarget(&mode);
		}
		else
		{
			settings->save();
			settings->width = mFullscreenMode.Width;
			settings->height = mFullscreenMode.Height;

			mSwapChain->ResizeTarget(&mFullscreenMode);
			mSwapChain->SetFullscreenState(TRUE, NULL);
		}

		onResize();
		settings->fullscreen = !settings->fullscreen;
	}

	void Renderer::toggleEffect(int effect, bool active)
	{
		if(active)
			mEffects[effect]->enable(md3dDevice.Get());
		else
			mEffects[effect]->disable(md3dDevice.Get());
	}

	void Renderer::drawEffects(int from, int to, bool first)
	{
		PostProcessing::begin(first ? 0 : swapChainBufferCount);
		for(int i = 0; i < swapChainBufferCount; ++i)
		{
			for(int j = from; j < to; ++j)
			{
				if(mEffects[j]->isActive())
				{
					int index = first ? i : (swapChainBufferCount + i);
					if(j == EFFECT_RESTIR_SPATIAL)
						mEffects[j]->effect(index, mCandidates.Get(), mCandidateHistory.Get());
					else
						mEffects[j]->effect(index, mSwapChainBuffer[i].Get());
				}
			}
		}
		PostProcessing::end(first ? 0 : swapChainBufferCount);
	}

	void Renderer::buildEffects()
	{
		Logger::INFO.log("Building ReSTIR spatial...");
		mEffects.push_back(std::make_unique<RestirSpatial>(md3dDevice.Get(), settings));

		Logger::INFO.log("Buliding FXAA effect...");
		mEffects.push_back(std::make_unique<FXAA>(md3dDevice.Get(), settings));

		Logger::INFO.log("Building color adjust effect...");
		ColorAdjustConfig config = { settings->exposure, settings->brightness, settings->contrast, settings->saturation, settings->gamma, (UINT) settings->tonemapping };
		auto ppColorAdjust = std::make_unique<ColorAdjust>(md3dDevice.Get(), settings, config);
		ppColorAdjust->enable(md3dDevice.Get());
		mEffects.push_back(std::move(ppColorAdjust));

		Logger::INFO.log("Building color grading effect...");
		mEffects.push_back(std::make_unique<ColorGrading>(md3dDevice.Get(), settings));

		Logger::INFO.log("Building vignette effect...");
		mEffects.push_back(std::make_unique<Vignette>(md3dDevice.Get(), settings));

		toggleEffect(EFFECT_FXAA, settings->fxaa && !settings->dlss);
		toggleEffect(EFFECT_COLOR_GRADING, settings->colorGrading);
		toggleEffect(EFFECT_VIGNETTE, settings->vignette);

		allocatePostProcessingResources();
	}

	void Renderer::allocatePostProcessingResources()
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		//color grading
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = mEffects[EFFECT_COLOR_GRADING]->getHeapCpu();
		srvDesc.Format = mLUT->Resource->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(mLUT->Resource.Get(), &srvDesc, handle);
	}

	bool Renderer::initDLSS()
	{
		int updateDriver = 0;
		UINT minDriverMajorI = 0;
		UINT minDriverMinorI = 0;
		UINT dlssSupported = 0;

		std::string version;
		std::transform(settings->version.begin(), settings->version.end(), std::back_inserter(version), [](wchar_t c) { return (char) c; });
		NVSDK_NGX_D3D12_Init_with_ProjectID(KEYS::projectKey, NVSDK_NGX_ENGINE_TYPE_CUSTOM, version.c_str(), L"", md3dDevice.Get());

		NVSDK_NGX_Result result = NVSDK_NGX_D3D12_GetCapabilityParameters(&mParams);
		if(result != NVSDK_NGX_Result_Success)
			return false;

		result = mParams->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &updateDriver);
		NVSDK_NGX_Result minDriverMajor = mParams->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverMajorI);
		NVSDK_NGX_Result minDriverMinor = mParams->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverMinorI);
		if(NVSDK_NGX_SUCCEED(result) && updateDriver && NVSDK_NGX_SUCCEED(minDriverMajor) && NVSDK_NGX_SUCCEED(minDriverMinor))
		{
			if(settings->dlss)
				MessageBox(nullptr, L"It seems that your NVIDIA drivers are out of date. This version of the driver does not support DLSS and it has been disabled.", L"Driver out of date", MB_OK | MB_ICONEXCLAMATION);
			return false;
		}

		//DLSS
		result = mParams->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			return false;
		result = mParams->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			return false;
		//DLSS RR
		settings->rayReconstructionSupported = true;
		result = mParams->Get(NVSDK_NGX_Parameter_SuperSamplingDenoising_Available, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			settings->rayReconstructionSupported = false;
		result = mParams->Get(NVSDK_NGX_Parameter_SuperSamplingDenoising_FeatureInitResult, &dlssSupported);
		if(NVSDK_NGX_FAILED(result) || !dlssSupported)
			settings->rayReconstructionSupported = false;

		if(!settings->rayReconstructionSupported)
		{
			Logger::WARN.log("Ray Reconstruction not supported");
			settings->rayReconstruction = false;
		}

		if(settings->dlss)
		{
			if(settings->rayReconstruction)
			{
				initRayReconstructionFeature();
				createDLSSResources();
			}
			else
			{
				initDLSSFeature();
				createDLSSResources();
			}
		}
		return true;
	}

	NVSDK_NGX_PerfQuality_Value Renderer::queryDLSSModeResolution()
	{
		UINT renderWMax, renderHMax, renderWMin, renderHMin;
		float sharpness = 0.0F;

		NVSDK_NGX_PerfQuality_Value mode;
		switch(settings->dlss)
		{
		default:
		case DLSS_PERFORMANCE:
			mode = NVSDK_NGX_PerfQuality_Value_MaxPerf;
			break;
		case DLSS_QUALITY:
			mode = NVSDK_NGX_PerfQuality_Value_MaxQuality;
			break;
		case DLSS_BALANCED:
			mode = NVSDK_NGX_PerfQuality_Value_Balanced;
			break;
		case DLSS_ULTRA_PERFORMANCE:
			mode = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
			break;
		case DLSS_ULTRA_QUALITY:
			mode = NVSDK_NGX_PerfQuality_Value_UltraQuality;
			break;
		case DLSS_DLAA:
			mode = NVSDK_NGX_PerfQuality_Value_DLAA;
			break;
		}

		NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(mParams, settings->width, settings->height, mode, &settings->dlssWidth, &settings->dlssHeight, &renderWMax, &renderHMax, &renderWMin, &renderHMin, &sharpness);

		if(settings->dlssWidth == 0 || settings->dlssHeight == 0)
			throw DLSSException("Unsupported DLSS mode");

		return mode;
	}

	void Renderer::initDLSSFeature()
	{
		NVSDK_NGX_PerfQuality_Value mode = queryDLSSModeResolution();

		int flags = NVSDK_NGX_DLSS_Feature_Flags_AutoExposure | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
		if(settings->backBufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;

		NVSDK_NGX_DLSS_Create_Params featureDesc = {};
		featureDesc.Feature.InWidth = settings->dlssWidth;
		featureDesc.Feature.InHeight = settings->dlssHeight;
		featureDesc.Feature.InTargetWidth = settings->width;
		featureDesc.Feature.InTargetHeight = settings->height;
		featureDesc.Feature.InPerfQualityValue = mode;
		featureDesc.InFeatureCreateFlags = flags;

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSS_EXT(mCommandList.Get(), 1, 1, &mFeature, mParams, &featureDesc);

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsLists);

		flushCommandQueue();
	}

	void Renderer::resetDLSSFeature()
	{
		if(mFeature)
			NVSDK_NGX_D3D12_ReleaseFeature(mFeature);
		initDLSSFeature();
	}

	void Renderer::initRayReconstructionFeature()
	{
		NVSDK_NGX_PerfQuality_Value mode = queryDLSSModeResolution();

		int flags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
		if(settings->backBufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;

		NVSDK_NGX_DLSSD_Create_Params featureDesc = {};
		featureDesc.InDenoiseMode = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;
		featureDesc.InRoughnessMode = NVSDK_NGX_DLSS_Roughness_Mode_Packed;
		featureDesc.InUseHWDepth = NVSDK_NGX_DLSS_Depth_Type_HW;
		featureDesc.InWidth = settings->dlssWidth;
		featureDesc.InHeight = settings->dlssHeight;
		featureDesc.InTargetWidth = settings->width;
		featureDesc.InTargetHeight = settings->height;
		featureDesc.InPerfQualityValue = mode;
		featureDesc.InFeatureCreateFlags = flags;
		featureDesc.InEnableOutputSubrects = false;

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSSD_EXT(mCommandList.Get(), 1, 1, &mFeature, mParams, &featureDesc);

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsLists);

		flushCommandQueue();
	}

	void Renderer::resetRayReconstructionFeature()
	{
		if(mFeature)
			NVSDK_NGX_D3D12_ReleaseFeature(mFeature);
		initRayReconstructionFeature();
	}

	void Renderer::createDLSSResources()
	{
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.DepthOrArraySize = 1;
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Format = settings->backBufferFormat;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		resDesc.Width = settings->width;
		resDesc.Height = settings->height;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.MipLevels = 1;
		md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mResolvedBuffer));
	}

	void Renderer::DLSS(ID3D12Resource* outputResource, ID3D12Resource* depthBuffer, DirectX::XMFLOAT2 jitter, bool reset)
	{
		NVSDK_NGX_D3D12_DLSS_Eval_Params evalDesc = {};
		evalDesc.Feature.pInColor = outputResource;
		evalDesc.Feature.pInOutput = mResolvedBuffer.Get();
		evalDesc.pInDepth = depthBuffer;
		evalDesc.pInMotionVectors = mMVBuffer.Get();
		if(settings->dlss != DLSS_DLAA)
		{
			evalDesc.InJitterOffsetX = -jitter.x * settings->getWidth();
			evalDesc.InJitterOffsetY = -jitter.y * settings->getHeight();
		}
		else
		{
			evalDesc.InJitterOffsetX = jitter.x * settings->getWidth();
			evalDesc.InJitterOffsetY = -jitter.y * settings->getHeight();
		}
		evalDesc.InRenderSubrectDimensions = { settings->dlssWidth, settings->dlssHeight };
		evalDesc.InReset = reset ? 1 : 0;
		evalDesc.InMVScaleX = -0.5F * settings->getWidth();
		evalDesc.InMVScaleY = 0.5F * settings->getHeight();

		NGX_D3D12_EVALUATE_DLSS_EXT(mDLSSCommandList.Get(), mFeature, mParams, &evalDesc);
	}

	void Renderer::rayReconstruction(ID3D12Resource* diffAlbedo, ID3D12Resource* specAlbedo, ID3D12Resource* normalsAndRough, ID3D12Resource* colorInput, ID3D12Resource* diffDist,
									 ID3D12Resource* specDist, float* viewMatrix, float* projMatrix, DirectX::XMFLOAT2 jitter, bool reset)
	{
		NVSDK_NGX_D3D12_DLSSD_Eval_Params evalDesc = {};
		evalDesc.pInDiffuseAlbedo = diffAlbedo;
		evalDesc.pInSpecularAlbedo = specAlbedo;
		evalDesc.pInNormals = normalsAndRough;
		evalDesc.pInColor = colorInput;
		evalDesc.pInMotionVectors = mMVBuffer.Get();
		evalDesc.pInDepth = mDepthStencilBuffer.Get();
		evalDesc.pInDiffuseHitDistance = diffDist;
		evalDesc.pInSpecularHitDistance = specDist;
		evalDesc.pInWorldToViewMatrix = viewMatrix;
		evalDesc.pInViewToClipMatrix = projMatrix;
		evalDesc.pInOutput = mResolvedBuffer.Get();
		evalDesc.InJitterOffsetX = -jitter.x * settings->getWidth();
		evalDesc.InJitterOffsetY = -jitter.y * settings->getHeight();
		evalDesc.InRenderSubrectDimensions = { settings->dlssWidth, settings->dlssHeight };
		evalDesc.InReset = reset ? 1 : 0;
		evalDesc.InMVScaleX = -0.5F * settings->getWidth();
		evalDesc.InMVScaleY = 0.5F * settings->getHeight();

		NGX_D3D12_EVALUATE_DLSSD_EXT(mDLSSCommandList.Get(), mFeature, mParams, &evalDesc);
	}

	void Renderer::walk(float dx)
	{
		mCam->walk(dx);
	}

	void Renderer::strafe(float dx)
	{
		mCam->strafe(dx);
	}

	void Renderer::update(float dt) {}

	void Renderer::buildDefaultFrameResources()
	{
		Logger::INFO.log("Building default frame resources...");
		for(int i = 0; i < NUM_FRAME_RESOURCES; ++i)
			frameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 0));
	}

	void Renderer::loadShadersAndInputLayout()
	{
		Logger::INFO.log("Building input layout...");
		mInputLayout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
	}

	void Renderer::loadLUT()
	{
		Logger::INFO.log("Loading LUT table...");
		mLUT = std::make_unique<Texture>();
		ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/luts/lut0.dds", mLUT->Resource, mLUT->UploadHeap));
	}

	void Renderer::loadBlueNoiseTexture()
	{
		Logger::INFO.log("Loading blue noise texture...");
		mBlueNoiseTex = std::make_unique<Texture>();
		ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/noise/blue_noise.dds", mBlueNoiseTex->Resource, mBlueNoiseTex->UploadHeap));
	}
}