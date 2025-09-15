#include "RaytracingRenderer.h"

#include "postprocessing/Vignette.h"

using namespace DirectX;

#define RAY_GEN_UAV_RES 15
#define NRD_SPLIT_SCREEN 0.0F

namespace RT
{
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> RaytracingRenderer::getStaticSamplers()
	{
		float lodOffset = 0.0F;
		if(settings->dlss)
			lodOffset = log2f((float) settings->dlssWidth / settings->width) - 1.0F;

		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, lodOffset, settings->anisotropic);
		const CD3DX12_STATIC_SAMPLER_DESC bilinearWrap(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, lodOffset, settings->anisotropic);
		const CD3DX12_STATIC_SAMPLER_DESC trilinearWrap(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, lodOffset, settings->anisotropic);
		return { pointWrap, bilinearWrap, trilinearWrap };
	}

	RaytracingRenderer::RaytracingRenderer(settings_struct* settings): Renderer(settings) {}

	RaytracingRenderer::~RaytracingRenderer()
	{
		if(mDenoiser)
			nrd::DestroyInstance(*mDenoiser);
	}

	bool RaytracingRenderer::initContext(std::string sceneName)
	{
		Logger::INFO.log("Initializing engine context...");

		Logger::INFO.log("Creating denoiser command list...");
		ThrowIfFailed(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(&mDenoiserCmdList)));
		mDenoiserCmdList->Close();

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		loadBlueNoiseTexture();
		loadLUT();
		if(settings->rayReconstruction)
			createCommonTextures();
		else if(!initDenoiser())
			return false;
		else
		{
			Logger::INFO.log("Initializing ray tracing composite effect...");
			mRTComposite = std::make_unique<RTComposite>(mDenoiserCmdList.Get(), md3dDevice.Get(), settings);
		}

		loadShadersAndInputLayout();
		buildMVRootSignature();
		buildMVPSOs();

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsList);

		flushCommandQueue();

		buildEffects();

		Logger::INFO.log("INITIALIZATION SUCCESSFUL!");

		Logger::INFO.log("Loading scene...");
		loadScene(sceneName);
		return true;
	}

	//init mvs
	void RaytracingRenderer::loadShadersAndInputLayout()
	{
		mMVShaders["mvVS"] = LoadBinary(L"res/shaders/bin/mv_vs.cso");
		mMVShaders["mvPS"] = LoadBinary(L"res/shaders/bin/mv_ps.cso");
		mMVShaders["mvAlphaTestedPS"] = LoadBinary(L"res/shaders/bin/mv_alpha_tested_ps.cso");

		Renderer::loadShadersAndInputLayout();
	}

	void RaytracingRenderer::buildMVRootSignature()
	{
		Logger::INFO.log("Building root signature...");

		CD3DX12_DESCRIPTOR_RANGE texTables;
		texTables.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[4];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsShaderResourceView(0, 2);
		slotRootParameter[2].InitAsShaderResourceView(1, 2);
		slotRootParameter[3].InitAsDescriptorTable(1, &texTables, D3D12_SHADER_VISIBILITY_PIXEL);

		auto samplers = getStaticSamplers();
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT) samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if(errorBlob != nullptr)
			::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
		ThrowIfFailed(hr);

		ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mMvSignature)));
	}

	void RaytracingRenderer::buildMVPSOs()
	{
		Logger::INFO.log("Building opaque pipeline state...");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
		{
			ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
			opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT) mInputLayout.size() };
			opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			opaquePsoDesc.SampleMask = UINT_MAX;
			opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			opaquePsoDesc.NumRenderTargets = 1;
			opaquePsoDesc.RTVFormats[0] = settings->backBufferFormat;
			opaquePsoDesc.SampleDesc.Count = 1;
			opaquePsoDesc.SampleDesc.Quality = 0;
			opaquePsoDesc.DSVFormat = mDepthStencilFormat;
		}

		Logger::INFO.log("Building motion vectors pipeline state...");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC drawMVPsoDesc = opaquePsoDesc;
		{
			drawMVPsoDesc.pRootSignature = mMvSignature.Get();
			drawMVPsoDesc.VS = {
				reinterpret_cast<BYTE*>(mMVShaders["mvVS"]->GetBufferPointer()),
				mMVShaders["mvVS"]->GetBufferSize()
			};
			drawMVPsoDesc.PS = {
				reinterpret_cast<BYTE*>(mMVShaders["mvPS"]->GetBufferPointer()),
				mMVShaders["mvPS"]->GetBufferSize()
			};
			drawMVPsoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			drawMVPsoDesc.SampleDesc.Count = 1;
			drawMVPsoDesc.SampleDesc.Quality = 0;
			drawMVPsoDesc.DSVFormat = mDepthStencilFormat;
			drawMVPsoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
			ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawMVPsoDesc, IID_PPV_ARGS(&mMVPSOs["mv"])));
		}

		Logger::INFO.log("Buliding alpha tested motion vectors pipeline state...");
		D3D12_GRAPHICS_PIPELINE_STATE_DESC atMVPsoDesc = drawMVPsoDesc;
		{
			atMVPsoDesc.PS = {
				reinterpret_cast<BYTE*>(mMVShaders["mvAlphaTestedPS"]->GetBufferPointer()),
				mMVShaders["mvAlphaTestedPS"]->GetBufferSize()
			};
			atMVPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&atMVPsoDesc, IID_PPV_ARGS(&mMVPSOs["mv_alpha_tested"])));
		}
	}

	//denoiser init sub-routines
	bool RaytracingRenderer::initDenoiser()
	{
		auto libDesc = nrd::GetLibraryDesc();
		Logger::INFO.log("Initializing NRD " + std::to_string(libDesc.versionMajor) + "." + std::to_string(libDesc.versionMinor) + "." + std::to_string(libDesc.versionBuild));

		nrd::DenoiserDesc methods[2];
		methods[0].denoiser = nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_SH;
		methods[0].identifier = 0;
		methods[1].denoiser = nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY;
		methods[1].identifier = 1;

		nrd::InstanceCreationDesc desc = {};
		desc.denoisersNum = 2;
		desc.denoisers = methods;

		if(nrd::CreateInstance(desc, mDenoiser) != nrd::Result::SUCCESS)
			return false;

		nrd::ReblurSettings s = {};
		nrd::SetDenoiserSettings(*mDenoiser, 0, &s);

		createDenoiserPipelines();
		createDenoiserResources();
		return true;
	}

	void RaytracingRenderer::createDenoiserPipelines()
	{
		Logger::INFO.log("Creating NRD specific pipelines...");

		const nrd::InstanceDesc desc = nrd::GetInstanceDesc(*mDenoiser);

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

			CD3DX12_ROOT_PARAMETER rootParameters[3];
			rootParameters[0].InitAsDescriptorTable(pipelineDesc.resourceRangesNum, ranges.data(), D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[1].InitAsDescriptorTable(1, &samplers, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[2].InitAsConstantBufferView(desc.constantBufferRegisterIndex, desc.constantBufferSpaceIndex, D3D12_SHADER_VISIBILITY_ALL);
			CD3DX12_ROOT_SIGNATURE_DESC sigDesc(3, rootParameters);

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

	void RaytracingRenderer::createDenoiserResources()
	{
		Logger::INFO.log("Creating NRD specific resources...");

		mDenoiserResources.clear();

		const nrd::InstanceDesc desc = nrd::GetInstanceDesc(*mDenoiser);
		UINT32 poolSize = desc.permanentPoolSize + desc.transientPoolSize;
		mDenoiserResources.resize(poolSize);

		//create cbv
		int constantBufferViewSize = CalcConstantBufferByteSize(desc.constantBufferMaxDataSize);
		int constantBufferSize = uint64_t(constantBufferViewSize) * desc.descriptorPoolDesc.setsMaxNum;
		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto rd = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mDenoiserCBV)));

		//create textures
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Alignment = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto hpd = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		for(UINT32 i = 0; i < poolSize; ++i)
		{
			const nrd::TextureDesc& textureDesc = (i < desc.permanentPoolSize) ? desc.permanentPool[i] : desc.transientPool[i - desc.permanentPoolSize];

			resDesc.Width = settings->getWidth() / textureDesc.downsampleFactor;
			resDesc.Height = settings->getHeight() / textureDesc.downsampleFactor;
			resDesc.Format = denoiserToDX(textureDesc.format);
			ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mDenoiserResources[i])));
		}

		createCommonTextures();

		//heaps creation
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = desc.samplersNum;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDenoiserSamplerHeap)));

		heapDesc.NumDescriptors = 35 * desc.descriptorPoolDesc.setsMaxNum;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDenoiserResourcesHeap)));

		//allocate samplers
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
			else if(desc.samplers[i] == nrd::Sampler::LINEAR_CLAMP)
			{
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}

			md3dDevice->CreateSampler(&samplerDesc, hDescriptor);
			hDescriptor.Offset(1, mSamplerDescriptorSize);
		}
	}

	void RaytracingRenderer::createCommonTextures()
	{
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Alignment = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		resDesc.Width = settings->getWidth();
		resDesc.Height = settings->getHeight();
		resDesc.Format = settings->backBufferFormat;

		auto hpd = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDiffuse)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSpecular)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDenoisedDiffuse)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDenoisedSpecular)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSkyBuffer)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mAlbedo)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mShadowTranslucency)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mShadowDenoised)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mRF0)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDiffSH1)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSpecSH1)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDiffSH1Denoised)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSpecSH1Denoised)));

		resDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mNormalRoughness)));

		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mZDepth)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mShadowData)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mDiffConfidence)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSpecConfidence)));

		resDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mMVCopy)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mViewAndRF0)));

		UINT stride = 2 * sizeof(float) + 2 * sizeof(UINT);
		UINT elements = settings->getWidth() * settings->getHeight();

		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Alignment = 0;
		resDesc.Width = stride * elements;
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = DXGI_FORMAT_UNKNOWN;
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mCandidates)));
		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hpd, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mCandidateHistory)));
	}

	void RaytracingRenderer::buildFrameResources()
	{
		Logger::INFO.log("Building frame resources...");
		frameResources.clear();
		std::vector<UINT16> instanceCount;
		for(auto& i:mScene->getAllEntities())
			instanceCount.push_back(i->getMaxInstances());
		for(int i = 0; i < NUM_FRAME_RESOURCES; ++i)
			frameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, mScene->getMaterialCount(), instanceCount));
	}

	void RaytracingRenderer::allocatePostProcessingResources()
	{
		Renderer::allocatePostProcessingResources();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Format = settings->backBufferFormat;

		//rt composite
		if(!settings->rayReconstruction)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mRTComposite->getHeapCpu());
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mDenoisedSpecular.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mSkyBuffer.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mAlbedo.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mShadowDenoised.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mRF0.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mDiffSH1Denoised.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			md3dDevice->CreateShaderResourceView(mSpecSH1Denoised.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			md3dDevice->CreateShaderResourceView(mNormalRoughness.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			md3dDevice->CreateShaderResourceView(mViewAndRF0.Get(), &srvDesc, handle);
			handle.Offset(1, mCbvSrvUavDescriptorSize);
			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			md3dDevice->CreateShaderResourceView(mZDepth.Get(), &srvDesc, handle);
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC buffer = {};
		buffer.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		buffer.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		buffer.Buffer.FirstElement = 0;
		buffer.Buffer.NumElements = settings->getWidth() * settings->getHeight();
		buffer.Buffer.StructureByteStride = 2 * sizeof(float) + 2 * sizeof(UINT);
		buffer.Format = DXGI_FORMAT_UNKNOWN;

		//restir spatial
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = mEffects[EFFECT_RESTIR_SPATIAL]->getHeapCpu();
		md3dDevice->CreateShaderResourceView(mCandidates.Get(), &buffer, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		md3dDevice->CreateShaderResourceView(mNormalRoughness.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		srvDesc.Format = settings->backBufferFormat;
		md3dDevice->CreateShaderResourceView(mRF0.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = settings->getWidth() * settings->getHeight();
		uavDesc.Buffer.StructureByteStride = 2 * sizeof(float) + 2 * sizeof(UINT);
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		md3dDevice->CreateUnorderedAccessView(mCandidateHistory.Get(), nullptr, &uavDesc, handle);
	}

	//ray tracing init sub-routines
	RaytracingRenderer::AccelerationStructureBuffers RaytracingRenderer::createBottomLevelAS(std::string name, const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vVertexBuffers,
																							 const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vIndexBuffers,
																							 bool alphaTested, bool allowUpdate, bool tessellated)
	{
		for(size_t i = 0; i < vVertexBuffers.size(); ++i)
			mBottomLevelAS[name].AddVertexBuffer(vVertexBuffers[i].first.Get(), 0, vVertexBuffers[i].second, sizeof(Vertex), vIndexBuffers[i].first.Get(), 0, vIndexBuffers[i].second, nullptr, 0, !alphaTested);

		UINT64 scratchSizeInBytes, resultSizeInBytes;
		mBottomLevelAS[name].ComputeASBufferSizes(md3dDevice.Get(), allowUpdate, &scratchSizeInBytes, &resultSizeInBytes);

		AccelerationStructureBuffers buffers;
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps, buffers.pScratch);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps, buffers.pResult);
		mBottomLevelAS[name].Generate(mCommandList.Get(), buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);
		return buffers;
	}

	void RaytracingRenderer::createTopLevelAS(const std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, XMMATRIX, bool, bool>>& instances)
	{
		for(size_t i = 0; i < instances.size(); ++i)
		{
			const auto& [res, world, opaque, shadowIgnore] = instances[i];
			UINT mask = 0xFF;
			if(shadowIgnore)
				mask = 0x01;
			mTopLevelASGenerator.AddInstance(res.Get(), world, static_cast<UINT>(i), static_cast<UINT>(i * 3), mask, opaque);
		}

		UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSize;
		mTopLevelASGenerator.ComputeASBufferSizes(md3dDevice.Get(), true, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSize);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pScratch);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pResult);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps, mTopLevelASBuffers.pInstanceDesc);
		mTopLevelASGenerator.Generate(mCommandList.Get(), mTopLevelASBuffers.pScratch.Get(), mTopLevelASBuffers.pResult.Get(), mTopLevelASBuffers.pInstanceDesc.Get());
	}

	void RaytracingRenderer::createAccelerationStructures()
	{
		Logger::INFO.log("Creating acceleration structures...");

		for(auto& data:mScene->getResidentGeometries())
			mBlbs[data->name] = createBottomLevelAS(data->name, { { data->VertexBufferGPU, data->vertexCount } }, { { data->IndexBufferGPU, data->DrawArgs["0"].IndexCount } }, false, data->isWater, false);

		for(auto& i:mScene->getAllEntities())
		{
			for(auto& inst:i->getInstances())
			{
				bool shadowIgnore = inst.emissiveIndex >= 0 || i->getType() == INSTANCE_TYPE_WATER;
				mInstances.push_back({ mBlbs[i->getGeo()->name].pResult, XMLoadFloat4x4(&inst.world), i->getLayer() == RenderLayer::Opaque, shadowIgnore });
			}
		}

		createTopLevelAS(mInstances);

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
		flushCommandQueue();
	}

	void RaytracingRenderer::createRayGenSignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
		rsc.AddHeapRangesParameter({ { 0, RAY_GEN_UAV_RES, 0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, RESERVED_SPACE },
									 { 0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RESERVED_SPACE + RAY_GEN_UAV_RES }, { 2, 3, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RESERVED_SPACE + RAY_GEN_UAV_RES + 2 } });
		auto samplers = getStaticSamplers();
		rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) samplers.size(), samplers.data());
	}

	void RaytracingRenderer::createMissSignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
		rsc.AddHeapRangesParameter({ { 0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, CUBEMAP_OFFSET } });
		auto samplers = getStaticSamplers();
		rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) samplers.size(), samplers.data());
	}

	void RaytracingRenderer::createHitSignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1, 1);
		rsc.AddHeapRangesParameter({ { 0, RESERVED_SPACE, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 }, { 2, 2, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RESERVED_SPACE + RAY_GEN_UAV_RES } });
		auto samplers = getStaticSamplers();
		rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) samplers.size(), samplers.data());
	}

	void RaytracingRenderer::createIndirectSignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1, 1);
		rsc.AddHeapRangesParameter({ { 0, 2, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 }, { 2, 1, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, EMISSIVE_OFFSET },
									 { 2, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, RESERVED_SPACE + RAY_GEN_UAV_RES } });
		auto samplers = getStaticSamplers();
		rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) samplers.size(), samplers.data());
	}

	void RaytracingRenderer::createEmptySignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.Generate(md3dDevice.Get(), true, pRootSig);
	}

	void RaytracingRenderer::createShadowHitSignature(ID3D12RootSignature** pRootSig)
	{
		nv_helpers_dx12::RootSignatureGenerator rsc;
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
		rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1, 1);
		rsc.AddHeapRangesParameter({ { 0, 1, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 } });
		auto samplers = getStaticSamplers();
		rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) samplers.size(), samplers.data());
	}

	void RaytracingRenderer::createRaytracingPipeline()
	{
		Logger::INFO.log("Creating ray tracing pipeline...");

		nv_helpers_dx12::RayTracingPipelineGenerator pipeline(md3dDevice.Get());
		mShaders["rayGen"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/raytracing/ray_gen.hlsl");
		mShaders["miss"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/raytracing/miss.hlsl");
		mShaders["closestHit"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/raytracing/hit.hlsl");
		mShaders["shadow"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/raytracing/shadow.hlsl");
		mShaders["indirect"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/raytracing/indirect.hlsl");

		pipeline.AddLibrary(mShaders["rayGen"].Get(), { L"RayGen" });
		pipeline.AddLibrary(mShaders["miss"].Get(), { L"Miss" });
		pipeline.AddLibrary(mShaders["closestHit"].Get(), { L"ClosestHit", L"AnyHit" });
		pipeline.AddLibrary(mShaders["shadow"].Get(), { L"ShadowHit", L"ShadowMiss", L"ShadowAnyHit" });
		pipeline.AddLibrary(mShaders["indirect"].Get(), { L"IndirectHit", L"IndirectMiss" });

		createRayGenSignature(&mSignatures["rayGen"]);
		createMissSignature(&mSignatures["miss"]);
		createHitSignature(&mSignatures["closestHit"]);
		createEmptySignature(&mSignatures["empty"]);
		createShadowHitSignature(&mSignatures["shadowHit"]);
		createIndirectSignature(&mSignatures["indirectHit"]);

		pipeline.AddHitGroup(L"HitGroup", L"ClosestHit", L"AnyHit");
		pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowHit", L"ShadowAnyHit");
		pipeline.AddHitGroup(L"IndirectHitGroup", L"IndirectHit");

		pipeline.AddRootSignatureAssociation(mSignatures["rayGen"].Get(), { L"RayGen" });
		pipeline.AddRootSignatureAssociation(mSignatures["miss"].Get(), { L"Miss" });
		pipeline.AddRootSignatureAssociation(mSignatures["closestHit"].Get(), { L"HitGroup" });
		pipeline.AddRootSignatureAssociation(mSignatures["empty"].Get(), { L"ShadowMiss", L"IndirectMiss" });
		pipeline.AddRootSignatureAssociation(mSignatures["shadowHit"].Get(), { L"ShadowHitGroup" });
		pipeline.AddRootSignatureAssociation(mSignatures["indirectHit"].Get(), { L"IndirectHitGroup" });

		pipeline.SetMaxPayloadSize(16 * sizeof(float) + 7 * sizeof(UINT));
		pipeline.SetMaxAttributeSize(2 * sizeof(float));
		pipeline.SetMaxRecursionDepth(4);

		pipeline.Generate(&mRtStateObject);
		ThrowIfFailed(mRtStateObject->QueryInterface(IID_PPV_ARGS(&mRtStateObjectProps)));
	}

	void RaytracingRenderer::createShaderBindingTable(bool reload)
	{
		if(!reload)
			Logger::INFO.log("Creating shader binding table...");

		D3D12_GPU_DESCRIPTOR_HANDLE heapHandle = mScene->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
		UINT64* heapPointer = reinterpret_cast<UINT64*>(heapHandle.ptr);

		for(int j = 0; j < NUM_FRAME_RESOURCES; ++j)
		{
			mSBTHelper.Reset();
			mSBTHelper.AddRayGenerationProgram(L"RayGen", {
				(void*) frameResources[j]->passCB->resource()->GetGPUVirtualAddress(),
				(void*) frameResources[j]->materialCB->resource()->GetGPUVirtualAddress(),
				heapPointer
			});
			mSBTHelper.AddMissProgram(L"Miss", {
				(void*) frameResources[j]->passCB->resource()->GetGPUVirtualAddress(),
				heapPointer
			});
			mSBTHelper.AddMissProgram(L"ShadowMiss", {});
			mSBTHelper.AddMissProgram(L"IndirectMiss", {});

			UINT count = 0;
			for(auto& i:mScene->getAllEntities())
			{
				UINT index = 0;
				for(auto& inst:i->getInstances())
				{
					if(i->isCulled(index++) && reload)
						continue;

					mSBTHelper.AddHitGroup(L"HitGroup", {
						(void*) frameResources[j]->passCB->resource()->GetGPUVirtualAddress(),
						(void*) i->getGeo()->VertexBufferGPU->GetGPUVirtualAddress(),
						(void*) i->getGeo()->IndexBufferGPU->GetGPUVirtualAddress(),
						(void*) frameResources[j]->materialCB->resource()->GetGPUVirtualAddress(),
						(void*) frameResources[j]->instanceBufferRT->resource()->GetGPUVirtualAddress(),
						heapPointer
					});
					mSBTHelper.AddHitGroup(L"ShadowHitGroup", {
						(void*) i->getGeo()->VertexBufferGPU->GetGPUVirtualAddress(),
						(void*) i->getGeo()->IndexBufferGPU->GetGPUVirtualAddress(),
						(void*) frameResources[j]->materialCB->resource()->GetGPUVirtualAddress(),
						(void*) frameResources[j]->instanceBufferRT->resource()->GetGPUVirtualAddress(),
						heapPointer
					});
					mSBTHelper.AddHitGroup(L"IndirectHitGroup", {
						(void*) frameResources[j]->passCB->resource()->GetGPUVirtualAddress(),
						(void*) i->getGeo()->VertexBufferGPU->GetGPUVirtualAddress(),
						(void*) i->getGeo()->IndexBufferGPU->GetGPUVirtualAddress(),
						(void*) frameResources[j]->materialCB->resource()->GetGPUVirtualAddress(),
						(void*) frameResources[j]->instanceBufferRT->resource()->GetGPUVirtualAddress(),
						heapPointer
					});
					count++;
				}
			}

			if(count == 0)
			{
				mSBTHelper.AddHitGroup(L"HitGroup", {});
				mSBTHelper.AddHitGroup(L"AOHitGroup", {});
				mSBTHelper.AddHitGroup(L"ShadowHitGroup", {});
			}

			UINT32 sbtSize0 = mSBTHelper.ComputeSBTSize();
			nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), sbtSize0, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps, frameResources[j]->SBTStorage);
			if(!frameResources[j]->SBTStorage)
				throw std::exception("Could not allocate shader binding table");
			mSBTHelper.Generate(frameResources[j]->SBTStorage.Get(), mRtStateObjectProps.Get());
		}
	}

	void RaytracingRenderer::allocateRaytracingResources()
	{
		Logger::INFO.log("Allocating ray tracing shader resources...");

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mScene->getLastCPUHeapAddress());

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		md3dDevice->CreateUnorderedAccessView(mDiffuse.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mSpecular.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mNormalRoughness.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mZDepth.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mSkyBuffer.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mAlbedo.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mShadowData.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mShadowTranslucency.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mRF0.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mDiffSH1.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mSpecSH1.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mViewAndRF0.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mDiffConfidence.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateUnorderedAccessView(mSpecConfidence.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = settings->getWidth() * settings->getHeight();
		uavDesc.Buffer.StructureByteStride = 2 * sizeof(float) + 2 * sizeof(UINT);
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		md3dDevice->CreateUnorderedAccessView(mCandidates.Get(), nullptr, &uavDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		//bvh
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.RaytracingAccelerationStructure.Location = mTopLevelASBuffers.pResult->GetGPUVirtualAddress();
		md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		//blue nosie
		srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = mBlueNoiseTex->Resource->GetDesc().Format;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		md3dDevice->CreateShaderResourceView(mBlueNoiseTex->Resource.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		//mv
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		md3dDevice->CreateShaderResourceView(mMVBuffer.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		//depth buffer
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		md3dDevice->CreateShaderResourceView(mDepthStencilBuffer.Get(), &srvDesc, handle);
		handle.Offset(1, mCbvSrvUavDescriptorSize);

		//history
		srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = settings->getWidth() * settings->getHeight();
		srvDesc.Buffer.StructureByteStride = 2 * sizeof(float) + 2 * sizeof(UINT);
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		md3dDevice->CreateShaderResourceView(mCandidateHistory.Get(), &srvDesc, handle);
	}

	void RaytracingRenderer::updateFrameData()
	{
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
		mCurrFrameResource = frameResources[mCurrFrameResourceIndex].get();

		{
			updateMaterialCB();
			updateBLAS();
			updateTLAS();

			if(settings->dlss)
				jitter = { (haltonSequence(2, phase + 1) - 0.5F) / settings->getWidth(), (haltonSequence(3, phase + 1) - 0.5F) / settings->getHeight() };

			mCam->updateViewMatrix();

			updateMainPassCB();
			updateObjCB();
			updateDenoiser();

			if(mMainPassCB.lightsCount > 1)
				((RestirSpatial*) mEffects[EFFECT_RESTIR_SPATIAL].get())->setData(mMainPassCB.invView, mMainPassCB.invProj, mCam->getPos3F(), mMainPassCB.frameIndex, mMainPassCB.lightsCount, settings->getWidth(), settings->getHeight(), &mMainPassCB.lights[0]);
		}
	}

	void RaytracingRenderer::draw()
	{
		WaitForSingleObjectEx(mFrameWaitable, 1000, true);

		denoiseAndComposite();
		if(PostProcessing::isDirty())
			drawEffects(0, (int) mEffects.size(), true);
		if(settings->dlss)
		{
			ThrowIfFailed(mCurrFrameResource->dlssCmdListAlloc->Reset());
			ThrowIfFailed(mDLSSCommandList->Reset(mCurrFrameResource->dlssCmdListAlloc.Get(), nullptr));

			if(settings->rayReconstruction)
			{
				XMFLOAT4X4 view = mCam->getView4x4();
				XMFLOAT4X4 proj = mCam->getProj4x4();
				rayReconstruction(mAlbedo.Get(), mShadowTranslucency.Get(), mNormalRoughness.Get(), mDiffuse.Get(), mDiffConfidence.Get(), mSpecConfidence.Get(),
									&view.m[0][0], &proj.m[0][0], jitter, false);
			}
			else
				DLSS(currentDLSSBuffer(), mDepthStencilBuffer.Get(), jitter, false);

			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(mResolvedBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
			};
			mDLSSCommandList->ResourceBarrier(2, barriers);
			mDLSSCommandList->CopyResource(currentBackBuffer(), mResolvedBuffer.Get());
			barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mResolvedBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mDLSSCommandList->ResourceBarrier(2, barriers);

			ThrowIfFailed(mDLSSCommandList->Close());
		}

		ThrowIfFailed(mCurrFrameResource->cmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mCurrFrameResource->cmdListAlloc.Get(), nullptr));

		{
			//motion vectors
			ThrowIfFailed(mCurrFrameResource->mvCmdListAlloc->Reset());
			ThrowIfFailed(mMVCommandList->Reset(mCurrFrameResource->mvCmdListAlloc.Get(), mMVPSOs["mv"].Get()));

			ID3D12DescriptorHeap* heaps[] = { mScene->getDescriptorHeap() };
			mMVCommandList->SetDescriptorHeaps(1, heaps);

			mMVCommandList->SetGraphicsRootSignature(mMvSignature.Get());

			if(mCurrFrameResource->materialCB)
				mMVCommandList->SetGraphicsRootShaderResourceView(1, mCurrFrameResource->materialCB->resource()->GetGPUVirtualAddress());
			mMVCommandList->SetGraphicsRootDescriptorTable(3, mScene->getDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());

			drawMVAndDepth();

			ThrowIfFailed(mMVCommandList->Close());

			//raytracing
			mCommandList->SetDescriptorHeaps(1, heaps);

			auto mSBTStorage = mCurrFrameResource->SBTStorage;

			D3D12_DISPATCH_RAYS_DESC desc = {};

			UINT32 rayGenerationSizeInBytes = mSBTHelper.GetRayGenSectionSize();
			desc.RayGenerationShaderRecord.StartAddress = mSBTStorage->GetGPUVirtualAddress();
			desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSizeInBytes;

			UINT32 missSectionSizeInBytes = mSBTHelper.GetMissSectionSize();
			desc.MissShaderTable.StartAddress = mSBTStorage->GetGPUVirtualAddress() + rayGenerationSizeInBytes;
			desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
			desc.MissShaderTable.StrideInBytes = mSBTHelper.GetMissEntrySize();

			UINT32 hitGroupSectionSizeInBytes = mSBTHelper.GetHitGroupSectionSize();
			desc.HitGroupTable.StartAddress = mSBTStorage->GetGPUVirtualAddress() + rayGenerationSizeInBytes + missSectionSizeInBytes;
			desc.HitGroupTable.SizeInBytes = hitGroupSectionSizeInBytes;
			desc.HitGroupTable.StrideInBytes = mSBTHelper.GetHitGroupEntrySize();

			desc.Width = settings->getWidth();
			desc.Height = settings->getHeight();
			desc.Depth = 1;

			mCommandList->SetPipelineState1(mRtStateObject.Get());
			mCommandList->DispatchRays(&desc);
		}

		ThrowIfFailed(mCommandList->Close());

		std::vector<ID3D12CommandList*> cmdsList;
		cmdsList.push_back(mMVCommandList.Get());
		cmdsList.push_back(mCommandList.Get());
		cmdsList.push_back(mDenoiserCmdList.Get());
		if(settings->dlss)
			cmdsList.push_back(mDLSSCommandList.Get());
		cmdsList.push_back(PostProcessing::getCommandList(mCurrBackBuffer));
		mCommandQueue->ExecuteCommandLists((UINT) cmdsList.size(), cmdsList.data());

		ThrowIfFailed(mSwapChain->Present(settings->vSync && settings->fullscreen ? 1 : 0, 0));
		mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;

		mCurrFrameResource->fence = ++mCurrentFence;
		mCommandQueue->Signal(mFence.Get(), mCurrentFence);

		mMainPassCB.frameIndex++;
		mCam->saveState();
		for(auto& e:mScene->getAllEntities())
			e->saveState();
		if(settings->dlss)
		{
			jitterPrev = jitter;
			phase++;
		}
		if(nrdSettings.accumulationMode == nrd::AccumulationMode::RESTART)
			nrdSettings.accumulationMode = nrd::AccumulationMode::CONTINUE;
	}

	void RaytracingRenderer::denoise()
	{
		const nrd::InstanceDesc desc = nrd::GetInstanceDesc(*mDenoiser);
		const nrd::DispatchDesc* dd;
		UINT32 num;
		nrd::Identifier i[2] = { 0, 1 };
		nrd::GetComputeDispatches(*mDenoiser, i, 2, dd, num);
		nrd::SetCommonSettings(*mDenoiser, nrdSettings);

		BYTE* cbvData;
		mDenoiserCBV->Map(0, nullptr, reinterpret_cast<void**>(&cbvData));

		int constantBufferViewSize = CalcConstantBufferByteSize(desc.constantBufferMaxDataSize);

		ID3D12DescriptorHeap* heaps[] = { mDenoiserResourcesHeap.Get(), mDenoiserSamplerHeap.Get() };
		mDenoiserCmdList->SetDescriptorHeaps(2, heaps);

		for(UINT32 i = 0; i < num; ++i)
		{
			const nrd::DispatchDesc d = dd[i];
			if(d.gridWidth == 0 || d.gridHeight == 0)
				continue;
			
			memcpy(&cbvData[i * constantBufferViewSize], d.constantBufferData, d.constantBufferDataSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mDenoiserResourcesHeap->GetCPUDescriptorHandleForHeapStart(), d.pipelineIndex * 35, mCbvSrvUavDescriptorSize);
			CD3DX12_GPU_DESCRIPTOR_HANDLE handleGPU(mDenoiserResourcesHeap->GetGPUDescriptorHandleForHeapStart(), d.pipelineIndex * 35, mCbvSrvUavDescriptorSize);

			std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
			for(UINT32 j = 0; j < d.resourcesNum; ++j)
			{
				//allocate all resources needed
				DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
				ID3D12Resource* tex = nullptr;
				bool transition = false;

				const nrd::ResourceDesc res = d.resources[j];
				if(res.type == nrd::ResourceType::PERMANENT_POOL)
				{
					tex = mDenoiserResources[res.indexInPool].Get();
					format = denoiserToDX(desc.permanentPool[res.indexInPool].format);
					transition = true;
				}
				else if(res.type == nrd::ResourceType::TRANSIENT_POOL)
				{
					tex = mDenoiserResources[desc.permanentPoolSize + res.indexInPool].Get();
					format = denoiserToDX(desc.transientPool[res.indexInPool].format);
					transition = true;
				}
				else if(res.type == nrd::ResourceType::IN_MV)
				{
					tex = mMVCopy.Get();
					format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::IN_NORMAL_ROUGHNESS)
				{
					tex = mNormalRoughness.Get();
					format = DXGI_FORMAT_R10G10B10A2_UNORM;
				}
				else if(res.type == nrd::ResourceType::IN_VIEWZ)
				{
					tex = mZDepth.Get();
					format = DXGI_FORMAT_R32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST || res.type == nrd::ResourceType::IN_DIFF_SH0)
				{
					tex = mDiffuse.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST || res.type == nrd::ResourceType::OUT_DIFF_SH0 || res.type == nrd::ResourceType::OUT_VALIDATION)
				{
					tex = mDenoisedDiffuse.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST || res.type == nrd::ResourceType::IN_SPEC_SH0)
				{
					tex = mSpecular.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST || res.type == nrd::ResourceType::OUT_SPEC_SH0)
				{
					tex = mDenoisedSpecular.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_PENUMBRA)
				{
					tex = mShadowData.Get();
					format = DXGI_FORMAT_R32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::IN_TRANSLUCENCY)
				{
					tex = mShadowTranslucency.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY)
				{
					tex = mShadowDenoised.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_DIFF_SH1)
				{
					tex = mDiffSH1.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_SPEC_SH1)
				{
					tex = mSpecSH1.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_DIFF_SH1)
				{
					tex = mDiffSH1Denoised.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::OUT_SPEC_SH1)
				{
					tex = mSpecSH1Denoised.Get();
					format = settings->backBufferFormat;
				}
				else if(res.type == nrd::ResourceType::IN_DIFF_CONFIDENCE)
				{
					tex = mDiffConfidence.Get();
					format = DXGI_FORMAT_R32_FLOAT;
				}
				else if(res.type == nrd::ResourceType::IN_SPEC_CONFIDENCE)
				{
					tex = mSpecConfidence.Get();
					format = DXGI_FORMAT_R32_FLOAT;
				}
				else
					throw std::exception("Unhandled reblur image");

				if(res.descriptorType == nrd::DescriptorType::TEXTURE)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Format = format;
					srvDesc.Texture2D.PlaneSlice = 0;
					srvDesc.Texture2D.MostDetailedMip = 0;
					srvDesc.Texture2D.MipLevels = 1;
					md3dDevice->CreateShaderResourceView(tex, &srvDesc, handle);

					if(transition)
						barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
				}
				else
				{
					D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
					uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uavDesc.Format = format;
					md3dDevice->CreateUnorderedAccessView(tex, nullptr, &uavDesc, handle);

					if(transition)
						barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
				}

				handle.Offset(1, mCbvSrvUavDescriptorSize);
			}

			if(barriers.size() > 0)
				mDenoiserCmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());

			mDenoiserCmdList->SetComputeRootSignature(mDenoiserRootSignatures[d.pipelineIndex].Get());

			mDenoiserCmdList->SetPipelineState(mDenoiserPipelines[d.pipelineIndex].Get());
			mDenoiserCmdList->SetComputeRootDescriptorTable(0, handleGPU);
			mDenoiserCmdList->SetComputeRootDescriptorTable(1, mDenoiserSamplerHeap->GetGPUDescriptorHandleForHeapStart());
			mDenoiserCmdList->SetComputeRootConstantBufferView(2, mDenoiserCBV->GetGPUVirtualAddress() + i * constantBufferViewSize);
			mDenoiserCmdList->Dispatch(d.gridWidth, d.gridHeight, 1);

			for(D3D12_RESOURCE_BARRIER& b:barriers)
			{
				b.Transition.StateBefore = b.Transition.StateAfter;
				b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
			}

			if(barriers.size() > 0)
				mDenoiserCmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());
		}

		mDenoiserCBV->Unmap(0, nullptr);
	}

	void RaytracingRenderer::denoiseAndComposite()
	{
		ThrowIfFailed(mCurrFrameResource->denoiserCmdListAlloc->Reset());
		ThrowIfFailed(mDenoiserCmdList->Reset(mCurrFrameResource->denoiserCmdListAlloc.Get(), nullptr));

		if(!settings->rayReconstruction)
		{
			denoise();
			mRTComposite->effect(0, mDenoisedDiffuse.Get(), settings->dlss ? currentDLSSBuffer() : currentBackBuffer());
		}

		ThrowIfFailed(mDenoiserCmdList->Close());
	}

	//MVs
	void RaytracingRenderer::drawEntities(RenderLayer layer, ID3D12GraphicsCommandList* cl)
	{
		ID3D12GraphicsCommandList* cmdList = cl ? cl : mCommandList.Get();

		for(auto& ri:mScene->getEntityLayer(layer))
		{
			if(ri->getGeo() != nullptr && ri->getInstanceCount() > 0)
			{
				auto vb = ri->getGeo()->VertexBufferView();
				auto ib = ri->getGeo()->IndexBufferView();

				cmdList->IASetVertexBuffers(0, 1, &vb);
				cmdList->IASetIndexBuffer(&ib);
				cmdList->IASetPrimitiveTopology(ri->getPrimitiveTopology());

				cmdList->SetGraphicsRootShaderResourceView(2, mCurrFrameResource->instanceBuffer[ri->getIndex()]->resource()->GetGPUVirtualAddress());
				cmdList->DrawIndexedInstanced(ri->getIndexCount(), ri->getInstanceCount(), ri->getStartIndex(), ri->getBaseVertex(), 0);
			}
		}
	}

	void RaytracingRenderer::drawMVAndDepth()
	{
		mMVCommandList->RSSetViewports(1, &mScreenViewport);
		mMVCommandList->RSSetScissorRects(1, &mScissorRect);

		auto t = CD3DX12_RESOURCE_BARRIER::Transition(mMVBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mMVCommandList->ResourceBarrier(1, &t);

		auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBufferCount * 2, mRtvDescriptorSize);
		auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());

		float clearValue[] = { 0.0F, 0.0F, 0.0F, 0.0F };
		mMVCommandList->ClearRenderTargetView(rtv, clearValue, 0, nullptr);

		mMVCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0F, 0, 0, nullptr);
		mMVCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);

		mMVCommandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->passCB->resource()->GetGPUVirtualAddress());

		drawEntities(RenderLayer::Opaque, mMVCommandList.Get());
		drawEntities(RenderLayer::Transparent, mMVCommandList.Get());
		drawEntities(RenderLayer::Water, mMVCommandList.Get());
		mMVCommandList->SetPipelineState(mMVPSOs["mv_alpha_tested"].Get());
		drawEntities(RenderLayer::AlphaTested, mMVCommandList.Get());

		CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(mMVBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(mMVCopy.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
		};
		mMVCommandList->ResourceBarrier(2, barriers);
		mMVCommandList->CopyResource(mMVCopy.Get(), mMVBuffer.Get());
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mMVBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mMVCopy.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		mMVCommandList->ResourceBarrier(2, barriers);
	}

	//update sub-routines
	void RaytracingRenderer::rebuildTLAS()
	{
		flushCommandQueue();

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		mTopLevelASGenerator.clearInstances();
		UINT indexVisible = 0;
		UINT index = 0;
		for(auto& i:mScene->getAllEntities())
		{
			UINT currentInstance = 0, globalInstance = 0;
			for(auto& inst:i->getInstances())
			{
				if(!i->isCulled(currentInstance++))
				{
					bool shadowIgnore = inst.emissiveIndex >= 0 || i->getType() == INSTANCE_TYPE_WATER;
					UINT mask = 0xFF;
					if(shadowIgnore)
						mask = 0x01;

					MeshGeometry* geo = i->getGeo();
					mTopLevelASGenerator.AddInstance(mBlbs[geo->name].pResult.Get(), XMLoadFloat4x4(&inst.world), static_cast<UINT>(indexVisible), static_cast<UINT>(index * 3), mask, i->getLayer() == RenderLayer::Opaque);
					indexVisible++;
				}

				i->refitted();
				globalInstance++;
				index++;
			}
		}

		//TODO add terrain chunks

		UINT64 scratchSizeInBytes, resultSizeInBytes, instanceDescsSize;
		mTopLevelASGenerator.ComputeASBufferSizes(md3dDevice.Get(), true, &scratchSizeInBytes, &resultSizeInBytes, &instanceDescsSize);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pScratch);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pResult);
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps, mTopLevelASBuffers.pInstanceDesc);
		mTopLevelASGenerator.Generate(mCommandList.Get(), mTopLevelASBuffers.pScratch.Get(), mTopLevelASBuffers.pResult.Get(), mTopLevelASBuffers.pInstanceDesc.Get());

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsList);

		flushCommandQueue();

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mScene->getLastCPUHeapAddress(), RAY_GEN_UAV_RES, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.RaytracingAccelerationStructure.Location = mTopLevelASBuffers.pResult->GetGPUVirtualAddress();
		md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, handle);
	}

	void RaytracingRenderer::updateBLAS()
	{
		for(auto& e:mScene->getResidentGeometries())
		{
			if(e->needsRefit)
			{
				mBottomLevelAS[e->name].updateVertexBuffer(e->VertexBufferGPU.Get(), 0, e->vertexCount, sizeof(Vertex),
																e->IndexBufferGPU.Get(), 0, e->DrawArgs["0"].IndexCount, nullptr, 0, !e->isWater);

				ThrowIfFailed(mDirectCmdListAlloc->Reset());
				ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
				mBottomLevelAS[e->name].Generate(mCommandList.Get(), mBlbs[e->name].pScratch.Get(), mBlbs[e->name].pResult.Get(), true, mBlbs[e->name].pResult.Get());

				ThrowIfFailed(mCommandList->Close());
				ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
				mCommandQueue->ExecuteCommandLists(1, cmdsList);
				flushCommandQueue();

				e->needsRefit = false;
			}
		}
	}

	void RaytracingRenderer::updateTLAS()
	{
		bool needsRefit = false;
		int index = 0;
		for(auto& e:mScene->getAllEntities())
		{
			if(e->needsRefit())
			{
				int instanceID = 0;
				for(auto& i:e->getInstances())
				{
					if(!e->isCulled(instanceID++))
					{
						mTopLevelASGenerator.updateWorld(index, XMLoadFloat4x4(&i.world));
						needsRefit = true;
						e->refitted();

						index++;
					}
				}
			}
			else
				index += e->getInstanceCount();
		}

		if(needsRefit)
		{
			ThrowIfFailed(mDirectCmdListAlloc->Reset());
			ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
			mTopLevelASGenerator.Generate(mCommandList.Get(), mTopLevelASBuffers.pScratch.Get(), mTopLevelASBuffers.pResult.Get(), mTopLevelASBuffers.pInstanceDesc.Get(), true, mTopLevelASBuffers.pResult.Get());
			ThrowIfFailed(mCommandList->Close());
			ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
			mCommandQueue->ExecuteCommandLists(1, cmdsList);
			flushCommandQueue();
		}
	}

	void RaytracingRenderer::updateMainPassCB()
	{
		XMMATRIX view = mCam->getView();
		XMMATRIX proj = mCam->getProj();

		if(mCam->isDirty())
		{
			mCam->cleanView();

			XMMATRIX invView = XMMatrixInverse(nullptr, view);
			XMStoreFloat4x4(&mMainPassCB.invView, invView);
			XMMATRIX invProj = XMMatrixInverse(nullptr, proj);
			XMStoreFloat4x4(&mMainPassCB.invProj, invProj);

			mMainPassCB.fov = 0.25F * XM_PI;
			mMainPassCB.aspectRatio = aspectRatio();

			UINT lightCount = 0;
			for(UINT i = 0; i < mScene->getLightCount(); ++i)
			{
				Light l = mScene->getLight(i);
				mMainPassCB.lights[lightCount++] = l;
				if(lightCount >= MAX_LIGHTS)
					break;
			}

			mMainPassCB.lightsCount = lightCount;
		}

		XMMATRIX viewPrev = mCam->getViewPrev();
		XMMATRIX projPrev = mCam->getProjPrev();
		XMMATRIX viewProjPrev = XMMatrixMultiply(viewPrev, projPrev);
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);

		XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));
		XMStoreFloat4x4(&mMainPassCB.viewPrev, XMMatrixTranspose(viewPrev));
		XMStoreFloat4x4(&mMainPassCB.viewProj, XMMatrixTranspose(viewProj));
		XMStoreFloat4x4(&mMainPassCB.viewProjPrev, XMMatrixTranspose(viewProjPrev));

		mMainPassCB.textureFilter = settings->texFilter;
		mMainPassCB.jitter = jitter;

		mMainPassCB.mipmaps = settings->mipmaps;

		mMainPassCB.rtao = settings->rtao;
		mMainPassCB.reflectionsRT = settings->rtReflections;
		mMainPassCB.refractionsRT = settings->rtRefractions;
		mMainPassCB.shadowsRT = settings->rtShadows;
		mMainPassCB.indirect = settings->indirect;

		mMainPassCB.texturing = settings->texturing;
		mMainPassCB.normalMapping = settings->normalMapping;
		mMainPassCB.roughnessMapping = settings->roughnessMapping;
		mMainPassCB.heightMapping = settings->heightMapping;
		mMainPassCB.AOMapping = settings->aoMapping;
		mMainPassCB.metallicMapping = settings->metallicMapping;
		mMainPassCB.specular = settings->specular;

		mMainPassCB.rayReconstruction = settings->rayReconstruction;

		mCurrFrameResource->passCB->copyData(0, mMainPassCB);
	}

	void RaytracingRenderer::updateObjCB()
	{
		//for mvs
		XMMATRIX view = mCam->getView();
		auto det = XMMatrixDeterminant(view);
		XMMATRIX invView = XMMatrixInverse(&det, view);
		bool oneCulled = false;

		int j = 0;
		for(auto& ri:mScene->getAllEntities())
		{
			const auto& instanceData = ri->getInstances();
			int count = 0;

			for(UINT i = 0; i < (UINT) instanceData.size(); ++i)
			{
				XMMATRIX world = XMLoadFloat4x4(&instanceData[i].world);
				det = XMMatrixDeterminant(world);
				XMMATRIX invWorld = XMMatrixInverse(&det, world);
				XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

				BoundingFrustum localSpaceFrustum;
				mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

				XMFLOAT3 instancePos = ri->getPos(i);
				float distance = XMVectorGetX(XMVector3Length(XMLoadFloat3(&instancePos) - mCam->getPos()));
				ri->setDistance(i, distance);

				BoundingBox shadowCullingBox = ri->getBounds();
				if(settings->rtShadows)
				{
					if(mMainPassCB.lightsCount > 0 && mMainPassCB.lights[0].lightType == LIGHT_TYPE_DIRECTIONAL)
					{
						const float scale = 7.5F;
						shadowCullingBox.Center.x += mMainPassCB.lights[0].Direction.x * scale;
						shadowCullingBox.Center.z += mMainPassCB.lights[0].Direction.z * scale;
						shadowCullingBox.Extents.x += fabsf(mMainPassCB.lights[0].Direction.x) * scale;
						shadowCullingBox.Extents.z += fabsf(mMainPassCB.lights[0].Direction.z) * scale;
					}
					else
					{
						shadowCullingBox.Extents.x += 0.1F;
						shadowCullingBox.Extents.z += 0.1F;
					}
				}
				if(localSpaceFrustum.Contains(shadowCullingBox) != DISJOINT || (settings->rtReflections && distance < 20.0F))
				{
					ObjectCB objCB;
					objCB.materialIndex = instanceData[i].materialIndex;
					objCB.textureIndex = instanceData[i].textureIndex;
					XMStoreFloat4x4(&objCB.world, XMMatrixTranspose(world));
					XMStoreFloat4x4(&objCB.prevWorld, XMMatrixTranspose(XMLoadFloat4x4(&instanceData[i].prevWorld)));
					mCurrFrameResource->instanceBuffer[ri->getIndex()]->copyData(count++, objCB);

					ObjectCB copy = instanceData[i];
					mCurrFrameResource->instanceBufferRT->copyData(j++, copy);

					if(ri->isCulled(i))
					{
						oneCulled = true;
						ri->setCulled(i, false);
					}
				}
				else if(!ri->isCulled(i))
				{
					oneCulled = true;
					ri->setCulled(i, true);
				}
			}

			ri->setInstanceCount(count);
		}

		if(oneCulled)
			rebuildTLAS();
	}

	void RaytracingRenderer::updateMaterialCB()
	{
		for(auto& m:mScene->getMaterials())
		{
			if(m->NumFramesDirty > 0)
			{
				MaterialConstants mat;
				mat.DiffuseAlbedo = m->DiffuseAlbedo;
				mat.FresnelR0 = m->FresnelR0;
				mat.Roughness = m->Roughness;
				XMStoreFloat4x4(&mat.MatTransform, XMMatrixTranspose(XMLoadFloat4x4(&m->MatTransform)));
				mat.metallic = m->metallic;
				mat.emission = m->emission;
				mat.refractionIndex = m->refractionIndex;
				mat.specular = m->specular;
				mat.castsShadows = m->castsShadows;

				mCurrFrameResource->materialCB->copyData(m->MatCBIndex, mat);
				m->NumFramesDirty--;
			}
		}
	}

	void RaytracingRenderer::updateDenoiser()
	{
		memcpy(nrdSettings.viewToClipMatrix, mCam->getProj4x4().m, sizeof(float) * 16);
		memcpy(nrdSettings.viewToClipMatrixPrev, mCam->getProj4x4Prev().m, sizeof(float) * 16);
		memcpy(nrdSettings.worldToViewMatrix, mCam->getView4x4().m, sizeof(float) * 16);
		memcpy(nrdSettings.worldToViewMatrixPrev, mCam->getView4x4Prev().m, sizeof(float) * 16);

		nrdSettings.cameraJitter[0] = jitter.x * settings->getWidth();
		nrdSettings.cameraJitter[1] = jitter.y * settings->getHeight();
		nrdSettings.cameraJitterPrev[0] = jitterPrev.x * settings->getWidth();
		nrdSettings.cameraJitterPrev[1] = jitterPrev.y * settings->getHeight();
		nrdSettings.resourceSize[0] = settings->getWidth();
		nrdSettings.resourceSize[1] = settings->getHeight();
		nrdSettings.resourceSizePrev[0] = settings->getWidth();
		nrdSettings.resourceSizePrev[1] = settings->getHeight();
		nrdSettings.rectSize[0] = settings->getWidth();
		nrdSettings.rectSize[1] = settings->getHeight();
		nrdSettings.rectSizePrev[0] = settings->getWidth();
		nrdSettings.rectSizePrev[1] = settings->getHeight();
		nrdSettings.rectOrigin[0] = 0;
		nrdSettings.rectOrigin[1] = 0;
		nrdSettings.frameIndex = mMainPassCB.frameIndex;
		nrdSettings.disocclusionThreshold = 0.01F;
		nrdSettings.denoisingRange = 999;
		nrdSettings.enableValidation = false;
		nrdSettings.isMotionVectorInWorldSpace = false;
		nrdSettings.isHistoryConfidenceAvailable = true;
		nrdSettings.splitScreen = NRD_SPLIT_SCREEN;
		nrdSettings.motionVectorScale[0] = -0.5F;
		nrdSettings.motionVectorScale[1] = 0.5F;
		nrdSettings.motionVectorScale[2] = 1.0F;
	}

	//other
	void RaytracingRenderer::onResize()
	{
		assert(md3dDevice);
		assert(mSwapChain);
		assert(mDirectCmdListAlloc);

		flushCommandQueue();

		if(settings->dlss)
		{
			if(settings->rayReconstruction)
				resetRayReconstructionFeature();
			else
				resetDLSSFeature();
			createDLSSResources();
		}

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		for(int i = 0; i < swapChainBufferCount; ++i)
		{
			D3D12_RESOURCE_DESC resDesc = {};
			resDesc.DepthOrArraySize = 1;
			resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resDesc.Format = settings->backBufferFormat;
			resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			resDesc.SampleDesc.Quality = 0;
			resDesc.SampleDesc.Count = 1;
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resDesc.MipLevels = 1;
			resDesc.Width = settings->getWidth();
			resDesc.Height = settings->getHeight();

			D3D12_CLEAR_VALUE optClear;
			optClear.Format = settings->backBufferFormat;
			optClear.Color[0] = 0.0F;
			optClear.Color[1] = 0.0F;
			optClear.Color[2] = 0.0F;
			optClear.Color[3] = 1.0F;

			auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			if(settings->dlss)
				md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &optClear, IID_PPV_ARGS(&mDLSSBuffers[i]));
			else
				mDLSSBuffers[i].Reset();
			mSwapChainBuffer[i].Reset();
		}

		mMVBuffer.Reset();

		ThrowIfFailed(mSwapChain->ResizeBuffers(swapChainBufferCount, settings->width, settings->height, settings->backBufferFormat, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
		mCurrBackBuffer = 0;

		//render targets
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
		for(int i = 0; i < swapChainBufferCount; ++i)
		{
			ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));

			md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, mRtvDescriptorSize);
		}

		if(settings->dlss)
		{
			for(int i = 0; i < swapChainBufferCount; ++i)
			{
				md3dDevice->CreateRenderTargetView(mDLSSBuffers[i].Get(), nullptr, rtvHeapHandle);
				rtvHeapHandle.Offset(1, mRtvDescriptorSize);
			}
		}
		else
			rtvHeapHandle.Offset(2, mRtvDescriptorSize);

		//motion vector
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.DepthOrArraySize = 1;
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resDesc.Width = settings->getWidth();
		resDesc.Height = settings->getHeight();
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.MipLevels = 1;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		optClear.Color[0] = 0.0F;
		optClear.Color[1] = 0.0F;
		optClear.Color[2] = 0.0F;
		optClear.Color[3] = 0.0F;

		auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&mMVBuffer));
		md3dDevice->CreateRenderTargetView(mMVBuffer.Get(), nullptr, rtvHeapHandle);

		//depth buffer
		D3D12_RESOURCE_DESC depthStencilDesc;
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = settings->getWidth();
		depthStencilDesc.Height = settings->getHeight();
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = mDepthStencilFormat;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		optClear.Format = mDepthStencilFormat;
		optClear.DepthStencil.Depth = 1.0F;
		optClear.DepthStencil.Stencil = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsv(mDsvHeap->GetCPUDescriptorHandleForHeapStart());

		ThrowIfFailed(md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear, IID_PPV_ARGS(&mDepthStencilBuffer)));
		md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, dsv);

		//execute
		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsList);

		//viewport & scissor
		mScreenViewport.TopLeftX = 0.0F;
		mScreenViewport.TopLeftY = 0.0F;
		mScreenViewport.Width = static_cast<float>(settings->getWidth());
		mScreenViewport.Height = static_cast<float>(settings->getHeight());
		mScreenViewport.MinDepth = 0.0F;
		mScreenViewport.MaxDepth = 1.0F;

		mScissorRect = { 0, 0, static_cast<LONG>(settings->getWidth()), static_cast<LONG>(settings->getHeight()) };

		flushCommandQueue();

		//application side
		if(mDenoiser)
		{
			nrd::DestroyInstance(*mDenoiser);
			initDenoiser();
		}
		else
			createCommonTextures();

		if(mScene)
			mScene->resizeCameras();
		mCamFrustum.CreateFromMatrix(mCamFrustum, mCam->getProj());

		Logger::INFO.log("Resizing effects...");
		if(mRTComposite)
			mRTComposite->onResize(md3dDevice.Get(), true);
		for(auto& e:mEffects)
			e->onResize(md3dDevice.Get(), true);
		if(mEffects.size() > 0)
		{
			allocatePostProcessingResources();
			((Vignette*) mEffects[EFFECT_VIGNETTE].get())->resetData();
		}

		mMainPassCB.frameIndex = 1;

		if(mDiffuse && mScene)
			allocateRaytracingResources();
	}

	void RaytracingRenderer::loadScene(std::string sceneName)
	{
		flushCommandQueue();

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		if(mScene)
			mScene.reset();
		mScene = std::make_unique<Scene>(md3dDevice.Get(), mCommandList.Get(), settings, "res/scenes/" + sceneName + ".uge");
		mScene->reloadMaterials();

		mCam = mScene->getSelectedCamera();

		createAccelerationStructures();

		ThrowIfFailed(mDirectCmdListAlloc->Reset());
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

		createRaytracingPipeline();
		buildFrameResources();
		createShaderBindingTable();
		allocateRaytracingResources();

		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(1, cmdsList);

		mCurrentFence = 0;
		mCurrFrameResourceIndex = 0;
		mCurrFrameResource = frameResources[0].get();
		nrdSettings.accumulationMode = nrd::AccumulationMode::RESTART;

		flushCommandQueue();
	}

	void RaytracingRenderer::createRtvAndDsvDescriptorHeaps()
	{
		/*
		* RTV0: back buffer 1
		* RTV1: back buffer 2
		* RTV2: dlss buffer 1
		* RTV3: dlss buffer 2
		* RTV4: motion vectors
		*/
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.NumDescriptors = swapChainBufferCount * 2 + 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

		/*
		* DSV0: depth stencil buffer
		*/

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
	}
}