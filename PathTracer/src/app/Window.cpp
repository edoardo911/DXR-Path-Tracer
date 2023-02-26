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
		nrd::DestroyDenoiser(*mDenoiser);
	}

	bool Window::initialize()
	{
		settings.load();

		if(!initMainWindow())
			return false;
		Logger::INFO.log("Initialized Window");

		if(!initDirectX12())
			return false;
		Logger::INFO.log("Initialized DirectX12");

		settings.dlssSupported = initDLSS();
		if(!settings.dlssSupported)
			settings.dlss = DLSS_OFF;
		phaseCount = settings.dlss ? (8 * (settings.width / settings.dlssWidth) * (settings.width / settings.dlssWidth)) : settings.RTAA;
		createDLSSResources();
		Logger::INFO.log("Initialized DLSS");

		if(!initDenoiser())
			return false;
		Logger::INFO.log("Initialized Denoiser");

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
			featureDesc.InFeatureCreateFlags = (settings.backBufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : NVSDK_NGX_DLSS_Feature_Flags_None)
												| NVSDK_NGX_DLSS_Feature_Flags_AutoExposure
												| NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
		
			ThrowIfFailed(mDirectCmdListAlloc->Reset());
			ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

			result = NGX_D3D12_CREATE_DLSS_EXT(mCommandList.Get(), 1, 1, &feature, params, &featureDesc);

			ThrowIfFailed(mCommandList->Close());
			ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
			mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
			
			phaseCount = (int) (8 * ((float) settings.width / settings.dlssWidth) * ((float) settings.width / settings.dlssWidth));
			flushCommandQueue();
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

	bool Window::initDenoiser()
	{
		nrd::MethodDesc methodDesc = {};
		methodDesc.method = nrd::Method::REBLUR_DIFFUSE;
		methodDesc.fullResolutionWidth = settings.dlss ? settings.dlssWidth : settings.width;
		methodDesc.fullResolutionHeight = settings.dlss ? settings.dlssHeight : settings.height;

		nrd::DenoiserCreationDesc desc = {};
		desc.requestedMethodsNum = 1;
		desc.requestedMethods = &methodDesc;

		if(nrd::CreateDenoiser(desc, mDenoiser) != nrd::Result::SUCCESS)
			return false;

		createDenoiserPipelines();
		createDenoiserResources();
		return true;
	}

	void Window::createDenoiserPipelines()
	{
		const nrd::DenoiserDesc desc = nrd::GetDenoiserDesc(*mDenoiser);

		mDenoiserPipelines.clear();
		mDenoiserRootSignatures.clear();
		mDenoiserPipelines.resize(desc.pipelinesNum);
		mDenoiserRootSignatures.resize(desc.pipelinesNum);

		CD3DX12_DESCRIPTOR_RANGE samplers;
		samplers.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, desc.samplersNum, desc.samplersBaseRegisterIndex, desc.samplersSpaceIndex);

		D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc = {};
		for(UINT32 i = 0; i < desc.pipelinesNum; ++i)
		{
			const nrd::PipelineDesc pipelineDesc = desc.pipelines[i];
			std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges(pipelineDesc.resourceRangesNum);

			for(UINT32 j = 0; j < pipelineDesc.resourceRangesNum; ++j)
			{
				const nrd::ResourceRangeDesc rangeDesc = pipelineDesc.resourceRanges[j];
				ranges[j].Init((rangeDesc.descriptorType == nrd::DescriptorType::TEXTURE) ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
							   rangeDesc.descriptorsNum, rangeDesc.baseRegisterIndex);
			}

			CD3DX12_ROOT_PARAMETER rootParameter[3];
			rootParameter[0].InitAsDescriptorTable(pipelineDesc.resourceRangesNum, ranges.data(), D3D12_SHADER_VISIBILITY_ALL);
			rootParameter[1].InitAsDescriptorTable(1, &samplers, D3D12_SHADER_VISIBILITY_ALL);
			rootParameter[2].InitAsConstantBufferView(desc.constantBufferRegisterIndex, desc.constantBufferSpaceIndex, D3D12_SHADER_VISIBILITY_ALL);
			CD3DX12_ROOT_SIGNATURE_DESC sigDesc(3, rootParameter);

			Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
			Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
			HRESULT hr = D3D12SerializeRootSignature(&sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

			if(errorBlob != nullptr)
				::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
			ThrowIfFailed(hr);

			ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mDenoiserRootSignatures[i])));

			computeDesc.pRootSignature = mDenoiserRootSignatures[i].Get();
			computeDesc.CS = {
				pipelineDesc.computeShaderDXIL.bytecode,
				pipelineDesc.computeShaderDXIL.size
			};
			ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&mDenoiserPipelines[i])));
		}
	}

	DXGI_FORMAT denoiserToDX(nrd::Format format)
	{
		switch(format)
		{
		default:
		case nrd::Format::R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;
		case nrd::Format::R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;
		case nrd::Format::R8_UINT:
			return DXGI_FORMAT_R8_UINT;
		case nrd::Format::R8_SINT:
			return DXGI_FORMAT_R8_SINT;
		case nrd::Format::RG8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case nrd::Format::RG8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;
		case nrd::Format::RG8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case nrd::Format::RG8_SINT:
			return DXGI_FORMAT_R8G8_SINT;
		case nrd::Format::RGBA8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case nrd::Format::RGBA8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		case nrd::Format::RGBA8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case nrd::Format::RGBA8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;
		case nrd::Format::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case nrd::Format::R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;
		case nrd::Format::R16_UINT:
			return DXGI_FORMAT_R16_UINT;
		case nrd::Format::R16_SINT:
			return DXGI_FORMAT_R16_SINT;
		case nrd::Format::R16_SFLOAT:
			return DXGI_FORMAT_R16_FLOAT;
		case nrd::Format::RG16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;
		case nrd::Format::RG16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;
		case nrd::Format::RG16_UINT:
			return DXGI_FORMAT_R16G16_UINT;
		case nrd::Format::RG16_SINT:
			return DXGI_FORMAT_R16G16_SINT;
		case nrd::Format::RG16_SFLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;
		case nrd::Format::RGBA16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case nrd::Format::RGBA16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;
		case nrd::Format::RGBA16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case nrd::Format::RGBA16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;
		case nrd::Format::RGBA16_SFLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case nrd::Format::R32_UINT:
			return DXGI_FORMAT_R32_UINT;
		case nrd::Format::R32_SINT:
			return DXGI_FORMAT_R32_SINT;
		case nrd::Format::R32_SFLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case nrd::Format::RG32_UINT:
			return DXGI_FORMAT_R32G32_UINT;
		case nrd::Format::RG32_SINT:
			return DXGI_FORMAT_R32G32_SINT;
		case nrd::Format::RG32_SFLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		case nrd::Format::RGB32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;
		case nrd::Format::RGB32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;
		case nrd::Format::RGB32_SFLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case nrd::Format::RGBA32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case nrd::Format::RGBA32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;
		case nrd::Format::RGBA32_SFLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case nrd::Format::R10_G10_B10_A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case nrd::Format::R10_G10_B10_A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;
		case nrd::Format::R11_G11_B10_UFLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;
		case nrd::Format::R9_G9_B9_E5_UFLOAT:
			return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		}
	}

	void Window::createDenoiserResources()
	{
		mDenoiserResources.clear();

		const nrd::DenoiserDesc desc = nrd::GetDenoiserDesc(*mDenoiser);
		UINT32 poolSize = desc.permanentPoolSize + desc.transientPoolSize;
		mDenoiserResources.resize(poolSize);

		//create constant buffer
		int constantBufferViewSize = CalcConstantBufferByteSize(desc.constantBufferMaxDataSize);
		int constantBufferSize = uint64_t(constantBufferViewSize) * desc.descriptorPoolDesc.setsMaxNum;
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto rd = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mDenoiserCBV)));

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.DepthOrArraySize = 1;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto hpd = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		for(UINT32 i = 0; i < poolSize; ++i)
		{
			//create textures
			const nrd::TextureDesc& textureDesc = (i < desc.permanentPoolSize) ? desc.permanentPool[i] : desc.transientPool[i - desc.permanentPoolSize];

			resDesc.Width = textureDesc.width;
			resDesc.Height = textureDesc.height;
			resDesc.MipLevels = textureDesc.mipNum;
			resDesc.Format = denoiserToDX(textureDesc.format);

			ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mDenoiserResources[i])));
		}

		resDesc.Width = settings.width;
		resDesc.Height = settings.height;
		resDesc.MipLevels = 1;
		resDesc.Format = settings.backBufferFormat;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDenoisedTexture[0])));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDenoisedTexture[1])));

		resDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mNormalRoughness)));

		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mZDepth)));

		//dh creation
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = desc.samplersNum;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDenoiserSamplerHeap)));

		heapDesc.NumDescriptors = 15;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDenoiserResourcesHeap)));

		//allocate
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.MaxLOD = 16;
		samplerDesc.MipLODBias = 0;
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mDenoiserSamplerHeap->GetCPUDescriptorHandleForHeapStart());
		for(UINT32 i = 0; i < desc.samplersNum; ++i)
		{
			if(desc.samplers[i] == nrd::Sampler::NEAREST_CLAMP)
			{
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			}
			else if(desc.samplers[i] == nrd::Sampler::NEAREST_MIRRORED_REPEAT)
			{
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			}
			else if(desc.samplers[i] == nrd::Sampler::LINEAR_CLAMP)
			{
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			else if(desc.samplers[i] == nrd::Sampler::LINEAR_MIRRORED_REPEAT)
			{
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}

			md3dDevice->CreateSampler(&samplerDesc, hDescriptor);
			hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		}
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
		resDesc.Width = settings.dlss ? settings.dlssWidth : settings.width;
		resDesc.Height = settings.dlss ? settings.dlssHeight : settings.height;
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

	void Window::DLSS(ID3D12Resource* outputResource, float jitterX, float jitterY, bool reset)
	{
		NVSDK_NGX_D3D12_DLSS_Eval_Params evalDesc = {};
		evalDesc.Feature.pInColor = outputResource;
		evalDesc.Feature.pInOutput = mResolvedBuffer.Get();
		evalDesc.pInDepth = mDepthBuffer.Get();
		evalDesc.pInMotionVectors = mMotionVectorBuffer.Get();
		evalDesc.InJitterOffsetX = -jitterX;
		evalDesc.InJitterOffsetY = -jitterY;
		evalDesc.InRenderSubrectDimensions = { settings.dlssWidth, settings.dlssHeight };
		evalDesc.InReset = reset ? 1 : 0;
		evalDesc.InFrameTimeDeltaInMsec = mTimer.deltaTime();
		
		NGX_D3D12_EVALUATE_DLSS_EXT(mCommandList.Get(), feature, params, &evalDesc);
	}

	void Window::denoise(DirectX::XMFLOAT4X4 proj, DirectX::XMFLOAT4X4 view, float jitterX, float jitterY, int frameIndex, ID3D12Resource* outputResource)
	{
		nrd::CommonSettings settings;
		for(int i = 0; i < 4; ++i)
		{
			for(int j = 0; j < 4; ++j)
			{
				settings.viewToClipMatrix[i * 4 + j] = proj(i, j);
				settings.viewToClipMatrixPrev[i * 4 + j] = proj(i, j);
				settings.worldToViewMatrix[i * 4 + j] = view(i, j);
				settings.worldToViewMatrixPrev[i * 4 + j] = view(i, j);
			}
		}

		settings.cameraJitter[0] = jitterX;
		settings.cameraJitter[1] = jitterY;
		settings.inputSubrectOrigin[0] = 0;
		settings.inputSubrectOrigin[1] = 0;
		settings.timeDeltaBetweenFrames = mTimer.deltaTime();
		settings.frameIndex = frameIndex;
		settings.disocclusionThreshold = 0.5F;
		settings.enableValidation = true;
		settings.denoisingRange = 9000;

		const nrd::DenoiserDesc desc = nrd::GetDenoiserDesc(*mDenoiser);
		const nrd::DispatchDesc* dd;
		uint32_t num;
		nrd::GetComputeDispatches(*mDenoiser, settings, dd, num);

		BYTE* cbvData;
		mDenoiserCBV->Map(0, nullptr, reinterpret_cast<void**>(&cbvData));

		int constantBufferViewSize = CalcConstantBufferByteSize(desc.constantBufferMaxDataSize);

		for(UINT32 i = 0; i < num; ++i)
		{
			const nrd::DispatchDesc d = dd[i];

			memcpy(&cbvData[i * constantBufferViewSize], d.constantBufferData, d.constantBufferDataSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mDenoiserResourcesHeap->GetCPUDescriptorHandleForHeapStart());
			for(UINT32 j = 0; j < d.resourcesNum; ++j)
			{
				DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
				ID3D12Resource* tex = mDenoiserResources[1].Get();
				
				const nrd::ResourceDesc res = d.resources[j];
				if(res.type == nrd::ResourceType::PERMANENT_POOL)
				{
					tex = mDenoiserResources[res.indexInPool].Get();
					format = denoiserToDX(desc.permanentPool[res.indexInPool].format);
				}
				else if(res.type == nrd::ResourceType::TRANSIENT_POOL)
				{
					tex = mDenoiserResources[desc.permanentPoolSize + res.indexInPool].Get();
					format = denoiserToDX(desc.transientPool[res.indexInPool].format);
				}
				else if(res.type == nrd::ResourceType::IN_MV)
				{
					tex = mMotionVectorBuffer.Get();
					format = DXGI_FORMAT_R32G32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::IN_VIEWZ)
				{
					tex = mZDepth.Get();
					format = DXGI_FORMAT_R32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST)
				{
					tex = mDenoisedTexture[0].Get();
					format = this->settings.backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_VALIDATION)
				{
					tex = mDenoisedTexture[1].Get();
					format = this->settings.backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST)
				{
					tex = outputResource;
					format = this->settings.backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_NORMAL_ROUGHNESS)
				{
					tex = mNormalRoughness.Get();
					format = DXGI_FORMAT_R10G10B10A2_UNORM;
				}
				else
					::OutputDebugString(L"Error");

				if(res.stateNeeded == nrd::DescriptorType::TEXTURE)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Format = format;
					srvDesc.Texture2D.PlaneSlice = 0;
					srvDesc.Texture2D.MostDetailedMip = res.mipOffset;
					srvDesc.Texture2D.MipLevels = res.mipNum;
					md3dDevice->CreateShaderResourceView(tex, &srvDesc, hDescriptor);
				}
				else
				{
					D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
					uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uavDesc.Format = format;
					md3dDevice->CreateUnorderedAccessView(tex, nullptr, &uavDesc, hDescriptor);
				}

				hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
			}

			ID3D12DescriptorHeap* heaps0[] = { mDenoiserResourcesHeap.Get(), mDenoiserSamplerHeap.Get() };
			mCommandList->SetDescriptorHeaps(2, heaps0);
			mCommandList->SetComputeRootSignature(mDenoiserRootSignatures[d.pipelineIndex].Get());

			mCommandList->SetPipelineState(mDenoiserPipelines[d.pipelineIndex].Get());
			mCommandList->SetComputeRootDescriptorTable(0, mDenoiserResourcesHeap->GetGPUDescriptorHandleForHeapStart());
			mCommandList->SetComputeRootDescriptorTable(1, mDenoiserSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			mCommandList->SetComputeRootConstantBufferView(2, mDenoiserCBV->GetGPUVirtualAddress() + i * constantBufferViewSize);

			mCommandList->Dispatch(d.gridWidth, d.gridHeight, 1);
		}

		mDenoiserCBV->Unmap(0, nullptr);
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