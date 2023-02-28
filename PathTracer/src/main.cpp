#include "utils/header.h"

#include "logging/Logger.h"

#include "app/RaytracingInstance.h"
#include "app/Window.h"

#include "utils/GeometryGenerator.h"
#include "utils/TextureLoader.h"

#include "rendering/FrameResource.h"
#include "rendering/Camera.h"

#include "raytracing/ShaderBindingTableGenerator.h"
#include "raytracing/RaytracingPipelineGenerator.h"
#include "raytracing/BottomLevelASGenerator.h"
#include "raytracing/RootSignatureGenerator.h"
#include "raytracing/TopLevelASGenerator.h"
#include "raytracing/DXRHelper.h"

using namespace RT;
using namespace DirectX;

class App: public Window
{
public:
	App(HINSTANCE hInstance);
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	~App();

	bool initialize() override;
private:
	void update() override;
	void draw() override;
	void onResize() override;

	void allocateOutputResources();

	void keyboardInput();
	void mouseInput();

	void updateMainPassCB();
	void updateObjCBs();
	void updateMaterialCBs();
	void updateDenoiser();

	struct AccelerationStructureBuffers
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
		Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
		Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc;
	};

	void loadTextures();
	void buildDescriptorHeap();
	void buildGeometries();
	void buildInstances();
	void buildMaterials();
	void buildOutputResource();

	AccelerationStructureBuffers createBottomLevelAS(const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vVertexBuffers,
							 const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vIndexBuffers, bool alphaTested);
	void createTopLevelAS(const std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, XMMATRIX, ObjectType>>& instances);
	void createAccelerationStructures();
	void createRayGenSignature(ID3D12RootSignature** pRootSig);
	void createMissSignature(ID3D12RootSignature** pRootSig);
	void createAOMissSignature(ID3D12RootSignature** pRootSig);
	void createHitSignature(ID3D12RootSignature** pRootSig);
	void createAOHitSignature(ID3D12RootSignature** pRootSig);
	void createShadowHitSignature(ID3D12RootSignature** pRootSig);
	void createPosHitSignature(ID3D12RootSignature** pRootSig);
	void createRaytracingPipeline();
	void createShaderBindingTable();

	void buildFrameResources();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> getStaticSamplers();

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 1;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOutputResource[2];

	std::unordered_map<std::string, std::unique_ptr<InstanceData>> mGeometries;
	std::vector<std::unique_ptr<RaytracingInstance>> mRTInstances;

	nv_helpers_dx12::TopLevelASGenerator mTopLevelASGenerator;
	AccelerationStructureBuffers mTopLevelASBuffers;
	std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, XMMATRIX, ObjectType>> mInstances;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mSignatures;
	Microsoft::WRL::ComPtr<ID3D12StateObject> mRtStateObject;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mRtStateObjectProps;

	nv_helpers_dx12::ShaderBindingTableGenerator mSBTHelper;
	HANDLE mFrameWaitable;

	std::unique_ptr<Camera> mCam;

	std::unique_ptr<Texture> mTexture;
	std::unique_ptr<Texture> mCubemap;
	std::unique_ptr<Texture> mNormalMap[2];

	std::vector<std::unique_ptr<Material>> mMaterials;

	MainPass mMainPassCB;
	UINT mFrameIndex = 0;

	bool mCameraMode = false;

	int phase = 0;
	XMFLOAT2 jitter = { 0, 0 };

	nrd::CommonSettings nrdSettings;
};

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstance, _In_ PSTR cmdLine, _In_ int showCmd)
{
	try
	{
		App app(hInstance);
		if(!app.initialize())
			return EXIT_FAILURE;
		return app.run();
	}
	catch(DLSSException e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		MessageBox(0, errorString.c_str(), L"DLSS Exception", MB_OK);
		return -3;
	}
	catch(RaytracingException e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		MessageBox(0, errorString.c_str(), L"Raytracing Exception", MB_OK);
		return -2;
	}
	catch(DxException e)
	{
		MessageBox(0, e.ToString().c_str(), L"DirectX Exception", MB_OK);
		return -1;
	}
	catch(std::exception e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		MessageBox(0, errorString.c_str(), L"Generic Exception", MB_OK);
		return 1;
	}

	return 0;
}

App::App(HINSTANCE hInstance): Window(hInstance)
{
	mCam = std::make_unique<Camera>(aspectRatio());
	mCam->setPos(-2.3F, 1.0F, -6.0F);
	mCam->lookAt(mCam->getPos(), XMVectorSet(-0.3F, 0.2F, 1.0F, 0.0F), XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
}

App::~App()
{
	if(md3dDevice)
		flushCommandQueue();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> App::getStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC bilinearWrap(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	const CD3DX12_STATIC_SAMPLER_DESC trilinearWrap(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	return { pointWrap, bilinearWrap, trilinearWrap };
}

bool App::initialize()
{
	if(!Window::initialize())
		return false;

	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	buildGeometries();
	Logger::INFO.log("Initialized Geometries");
	buildInstances();
	Logger::INFO.log("Initialized Instances");
	buildMaterials();
	Logger::INFO.log("Initialized Materials");
	buildOutputResource();
	Logger::INFO.log("Initialized Output Resources");

	createAccelerationStructures();
	Logger::INFO.log("Initialized Acceleration Structure");
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	createRaytracingPipeline();
	Logger::INFO.log("Initialized Ray Tracing Pipeline");

	Logger::INFO.log("Loading Textures...");
	loadTextures();
	Logger::INFO.log("Textures Loaded");
	buildDescriptorHeap();
	Logger::INFO.log("Initialized Descriptor Heap");
	buildFrameResources();
	Logger::INFO.log("Initialized Frame Resources");
	createShaderBindingTable();
	Logger::INFO.log("Initialized Shader Binding Table");

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

	flushCommandQueue();

	mFrameWaitable = mSwapChain->GetFrameLatencyWaitableObject();

	if(mCameraMode)
		toggleCursor(false);
	mouse.setPos(settings.width / 2, settings.height / 2);
	centerCursor();
	Logger::INFO.log("Initialization has succeeded. Starting game loop...");
	return true;
}

void App::buildGeometries()
{
	GeometryGenerator geoGen;

	auto sphere = geoGen.createSphere(0.5F, 50, 50);
	std::vector<Vertex> vertices(sphere.vertices.size());
	for(int i = 0; i < sphere.vertices.size(); ++i)
	{
		vertices[i].pos = sphere.vertices[i].position;
		vertices[i].uvs = sphere.vertices[i].texC;
		vertices[i].normal = sphere.vertices[i].normal;
		vertices[i].tangent = sphere.vertices[i].tangentU;
	}

	auto s = std::make_unique<InstanceData>("sphere");
	s->buildVertexBuffer(md3dDevice.Get(), vertices.data(), sizeof(Vertex) * static_cast<UINT>(vertices.size()));
	s->buildIndexBuffer(md3dDevice.Get(), sphere.indices32.data(), sizeof(UINT32) * static_cast<UINT>(sphere.indices32.size()));
	s->vertexCount = static_cast<UINT>(vertices.size());
	s->indexCount = static_cast<UINT>(sphere.indices32.size());
	mGeometries["sphere"] = std::move(s);

	auto grid = geoGen.createGrid(7.0F, 7.0F, 2, 2);
	std::vector<Vertex> gridVertices(grid.vertices.size());
	for(int i = 0; i < grid.vertices.size(); ++i)
	{
		gridVertices[i].pos = grid.vertices[i].position;
		gridVertices[i].uvs = grid.vertices[i].texC;
		gridVertices[i].normal = grid.vertices[i].normal;
		gridVertices[i].tangent = grid.vertices[i].tangentU;
	}

	auto g = std::make_unique<InstanceData>("grid");
	g->buildVertexBuffer(md3dDevice.Get(), gridVertices.data(), sizeof(Vertex) * static_cast<UINT>(gridVertices.size()));
	g->buildIndexBuffer(md3dDevice.Get(), grid.indices32.data(), sizeof(UINT32) * static_cast<UINT>(grid.indices32.size()));
	g->vertexCount = static_cast<UINT>(gridVertices.size());
	g->indexCount = static_cast<UINT>(grid.indices32.size());
	mGeometries["grid"] = std::move(g);

	auto quad = geoGen.createQuad(-3.5F, 2.5F, 7.0F, 3.0F, 2.0F);
	std::vector<Vertex> quadVertices(quad.vertices.size());
	for(int i = 0; i < quad.vertices.size(); ++i)
	{
		quadVertices[i].pos = quad.vertices[i].position;
		quadVertices[i].uvs = quad.vertices[i].texC;
		quadVertices[i].normal = quad.vertices[i].normal;
		quadVertices[i].tangent = quad.vertices[i].tangentU;
	}

	auto q = std::make_unique<InstanceData>("quad");
	q->buildVertexBuffer(md3dDevice.Get(), quadVertices.data(), sizeof(Vertex) * static_cast<UINT>(quadVertices.size()));
	q->buildIndexBuffer(md3dDevice.Get(), quad.indices32.data(), sizeof(UINT32) * static_cast<UINT>(quad.indices32.size()));
	q->vertexCount = static_cast<UINT>(quadVertices.size());
	q->indexCount = static_cast<UINT>(quad.indices32.size());
	mGeometries["quad"] = std::move(q);
}

void App::buildInstances()
{
	auto s0 = std::make_unique<RaytracingInstance>(mGeometries["sphere"].get());
	s0->world = XMMatrixTranslation(1.0F, 0.0F, 0.0F);
	s0->matOffset = 3;
	mRTInstances.push_back(std::move(s0));

	auto s1 = std::make_unique<RaytracingInstance>(mGeometries["sphere"].get());
	s1->objCBOffset = 1;
	s1->normalOffset = 1;
	s1->world = XMMatrixTranslation(-1.0F, 0.01F, 1.0F);
	s1->matOffset = 1;
	mRTInstances.push_back(std::move(s1));

	auto floor = std::make_unique<RaytracingInstance>(mGeometries["grid"].get());
	floor->objCBOffset = 2;
	floor->texOffset = 0;
	floor->normalOffset = 0;
	floor->world = XMMatrixTranslation(0.0F, -0.5F, -1.5F);
	mRTInstances.push_back(std::move(floor));

	auto ceil = std::make_unique<RaytracingInstance>(mGeometries["grid"].get());
	ceil->objCBOffset = 3;
	ceil->world = XMMatrixRotationAxis(XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), XM_PI) * XMMatrixTranslation(0.0F, 2.5F, -1.5F);
	mRTInstances.push_back(std::move(ceil));

	auto wallf = std::make_unique<RaytracingInstance>(mGeometries["quad"].get());
	wallf->objCBOffset = 4;
	wallf->matOffset = 2;
	mRTInstances.push_back(std::move(wallf));
}

void App::buildMaterials()
{
	auto generic = std::make_unique<Material>();
	mMaterials.push_back(std::move(generic));

	auto redBall = std::make_unique<Material>();
	redBall->DiffuseAlbedo = { 1.0F, 0.0F, 0.0F, 0.2F };
	redBall->matCBIndex = 1;
	redBall->Roughness = 0.05F;
	redBall->refractionIndex = 0.4F;
	mMaterials.push_back(std::move(redBall));

	auto wall = std::make_unique<Material>();
	wall->DiffuseAlbedo = { 0.0F, 1.0F, 0.0F, 1.0F };
	wall->matCBIndex = 2;
	wall->Roughness = 0.2F;
	mMaterials.push_back(std::move(wall));

	auto metallicBall = std::make_unique<Material>();
	//metallicBall->DiffuseAlbedo = { 1.0F, 1.0F, 1.0F, 0.1F };
	metallicBall->metallic = 0.8F;
	metallicBall->matCBIndex = 3;
	metallicBall->Roughness = 0.3F;
	mMaterials.push_back(std::move(metallicBall));
}

void App::buildOutputResource()
{
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = settings.backBufferFormat;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = settings.dlss ? settings.dlssWidth : settings.width;
	resDesc.Height = settings.dlss ? settings.dlssHeight : settings.height;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;

	auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, settings.dlss ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COPY_SOURCE,
				  nullptr, IID_PPV_ARGS(&mOutputResource[0])));

	resDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mOutputResource[1])));
}

App::AccelerationStructureBuffers App::createBottomLevelAS(const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vVertexBuffers,
						 const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, UINT32>>& vIndexBuffers, bool alphaTested)
{
	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;
	for(size_t i = 0; i < vVertexBuffers.size(); ++i)
		bottomLevelAS.AddVertexBuffer(vVertexBuffers[i].first.Get(), 0, vVertexBuffers[i].second, sizeof(Vertex), vIndexBuffers[i].first.Get(), 0, vIndexBuffers[i].second, nullptr, 0, !alphaTested);

	UINT64 scratchSizeInBytes, resultSizeInBytes;
	bottomLevelAS.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

	AccelerationStructureBuffers buffers;
	nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps, buffers.pScratch);
	nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps, buffers.pResult);
	bottomLevelAS.Generate(mCommandList.Get(), buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);
	return buffers;
}

void App::createTopLevelAS(const std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, XMMATRIX, ObjectType>>& instances)
{
	for(size_t i = 0; i < instances.size(); ++i)
	{
		const auto& [res, world, type] = instances[i];
		mTopLevelASGenerator.AddInstance(res.Get(), world, static_cast<UINT>(i), static_cast<UINT>(i * 4), 0xFF);
	}

	UINT64 scratchSize, resultSize, instanceDescsSize;
	mTopLevelASGenerator.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSize, &resultSize, &instanceDescsSize);
	nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pScratch);
	nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps, mTopLevelASBuffers.pResult);
	nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps, mTopLevelASBuffers.pInstanceDesc);
	mTopLevelASGenerator.Generate(mCommandList.Get(), mTopLevelASBuffers.pScratch.Get(), mTopLevelASBuffers.pResult.Get(), mTopLevelASBuffers.pInstanceDesc.Get());
}

void App::createAccelerationStructures() 
{
	std::unordered_map<std::string, AccelerationStructureBuffers> blbs;
	for(auto& [name, data]:mGeometries)
		blbs[name] = createBottomLevelAS({ { data->vertexBuffer, data->vertexCount } }, { { data->indexBuffer, data->indexCount } }, data->alphaTested);

	for(auto& i:mRTInstances)
		mInstances.push_back({ blbs[i->buffers->name].pResult, i->world, i->type });
	createTopLevelAS(mInstances);

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	flushCommandQueue();
	blbs.clear();
}

void App::createRayGenSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
	rsc.AddHeapRangesParameter({ { 0, 7, 0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0 }, { 0, 2, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7 } });
	rsc.Generate(md3dDevice.Get(), true, pRootSig);
}

void App::createMissSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddHeapRangesParameter({ { 0, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 11 } });
	auto s = getStaticSamplers();
	rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) s.size(), s.data());
}

void App::createAOMissSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.Generate(md3dDevice.Get(), true, pRootSig);
}

void App::createHitSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0, 1);
	rsc.AddHeapRangesParameter({ { 2, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7 }, { 3, 3, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8 } });
	auto s = getStaticSamplers();
	rsc.Generate(md3dDevice.Get(), true, pRootSig, (UINT) s.size(), s.data());
}

void App::createAOHitSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.Generate(md3dDevice.Get(), true, pRootSig);
}

void App::createShadowHitSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
	rsc.Generate(md3dDevice.Get(), true, pRootSig);
}

void App::createPosHitSignature(ID3D12RootSignature** pRootSig)
{
	nv_helpers_dx12::RootSignatureGenerator rsc;
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0);
	rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1);
	rsc.Generate(md3dDevice.Get(), true, pRootSig);
}

void App::createRaytracingPipeline()
{
	nv_helpers_dx12::RayTracingPipelineGenerator pipeline(md3dDevice.Get());
	mShaders["aoMiss"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/ao_miss.hlsl");
	mShaders["aoClosestHit"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/ao_hit.hlsl");
	mShaders["rayGen"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/ray_gen.hlsl");
	mShaders["miss"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/miss.hlsl");
	mShaders["closestHit"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/hit.hlsl");
	mShaders["shadow"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/shadow.hlsl");
	mShaders["pos"] = nv_helpers_dx12::CompileShaderLibrary(L"res/shaders/pos.hlsl");

	pipeline.AddLibrary(mShaders["rayGen"].Get(), { L"RayGen" });
	pipeline.AddLibrary(mShaders["miss"].Get(), { L"Miss" });
	pipeline.AddLibrary(mShaders["closestHit"].Get(), { L"ClosestHit" });
	pipeline.AddLibrary(mShaders["aoMiss"].Get(), { L"AOMiss" });
	pipeline.AddLibrary(mShaders["aoClosestHit"].Get(), { L"AOClosestHit" });
	pipeline.AddLibrary(mShaders["shadow"].Get(), { L"ShadowClosestHit", L"ShadowMiss" });
	pipeline.AddLibrary(mShaders["pos"].Get(), { L"PosClosestHit", L"PosMiss" });

	createRayGenSignature(&mSignatures["rayGen"]);
	createMissSignature(&mSignatures["miss"]);
	createAOMissSignature(&mSignatures["aoMiss"]);
	createHitSignature(&mSignatures["closestHit"]);
	createAOHitSignature(&mSignatures["aoClosestHit"]);
	createShadowHitSignature(&mSignatures["shadowClosestHit"]);
	createPosHitSignature(&mSignatures["posClosestHit"]);

	pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
	pipeline.AddHitGroup(L"AOHitGroup", L"AOClosestHit");
	pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");
	pipeline.AddHitGroup(L"PosHitGroup", L"PosClosestHit");

	pipeline.AddRootSignatureAssociation(mSignatures["rayGen"].Get(), { L"RayGen" });
	pipeline.AddRootSignatureAssociation(mSignatures["miss"].Get(), { L"Miss" });
	pipeline.AddRootSignatureAssociation(mSignatures["aoMiss"].Get(), { L"AOMiss", L"ShadowMiss", L"PosMiss" });
	pipeline.AddRootSignatureAssociation(mSignatures["closestHit"].Get(), { L"HitGroup" });
	pipeline.AddRootSignatureAssociation(mSignatures["aoClosestHit"].Get(), { L"AOHitGroup" });
	pipeline.AddRootSignatureAssociation(mSignatures["shadowClosestHit"].Get(), { L"ShadowHitGroup" });
	pipeline.AddRootSignatureAssociation(mSignatures["posClosestHit"].Get(), { L"PosHitGroup" });

	pipeline.SetMaxPayloadSize(12 * sizeof(float) + 1 * sizeof(UINT));
	pipeline.SetMaxAttributeSize(2 * sizeof(float));
	pipeline.SetMaxRecursionDepth(4);

	pipeline.Generate(&mRtStateObject);
	ThrowIfFailed(mRtStateObject->QueryInterface(IID_PPV_ARGS(&mRtStateObjectProps)));
}

void App::createShaderBindingTable()
{
	D3D12_GPU_DESCRIPTOR_HANDLE heapHandle = mHeap->GetGPUDescriptorHandleForHeapStart();

	UINT64* heapPointer = reinterpret_cast<UINT64*>(heapHandle.ptr);
	UINT objCBSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for(int j = 0; j < NUM_FRAME_RESOURCES; ++j)
	{
		mSBTHelper.Reset();
		mSBTHelper.AddRayGenerationProgram(L"RayGen", { (void*) mFrameResources[j]->passCB->resource()->GetGPUVirtualAddress(), heapPointer });
		mSBTHelper.AddMissProgram(L"Miss", {});
		mSBTHelper.AddMissProgram(L"ShadowMiss", {});
		mSBTHelper.AddMissProgram(L"AOMiss", {});
		mSBTHelper.AddMissProgram(L"PosMiss", {});

		for(auto& i:mRTInstances)
		{
			auto objCB = mFrameResources[j]->objCB->resource();
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objCB->GetGPUVirtualAddress() + i->objCBOffset * objCBSize;

			mSBTHelper.AddHitGroup(L"HitGroup", {
									(void*) mFrameResources[j]->passCB->resource()->GetGPUVirtualAddress(),
									(void*) objCBAddress,
									(void*) (i->buffers->vertexBuffer->GetGPUVirtualAddress()),
									(void*) (i->buffers->indexBuffer->GetGPUVirtualAddress()),
									(void*) (mFrameResources[j]->matCB->resource()->GetGPUVirtualAddress()),
									heapPointer
								   });
			mSBTHelper.AddHitGroup(L"ShadowHitGroup", {
									(void*) objCBAddress,
									(void*) (mFrameResources[j]->matCB->resource()->GetGPUVirtualAddress())
								   });
			mSBTHelper.AddHitGroup(L"AOHitGroup", {});
			mSBTHelper.AddHitGroup(L"PosHitGroup", {
									(void*) (i->buffers->vertexBuffer->GetGPUVirtualAddress()),
									(void*) (i->buffers->indexBuffer->GetGPUVirtualAddress()),
								   });
		}

		UINT32 sbtSize0 = mSBTHelper.ComputeSBTSize();
		nv_helpers_dx12::CreateBuffer(md3dDevice.Get(), sbtSize0, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps, mFrameResources[j]->SBTStorage);
		if(!mFrameResources[j]->SBTStorage)
			throw std::exception("Could not allocate shader binding table");
		mSBTHelper.Generate(mFrameResources[j]->SBTStorage.Get(), mRtStateObjectProps.Get());
	}
}

void App::loadTextures()
{
	auto tex = std::make_unique<Texture>();
	ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/textures/wood.dds", tex->Resource, tex->UploadHeap));
	mTexture = std::move(tex);

	auto nmap = std::make_unique<Texture>();
	ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/textures/wood_nm.dds", nmap->Resource, nmap->UploadHeap));
	mNormalMap[0] = std::move(nmap);

	auto nmap2 = std::make_unique<Texture>();
	ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/textures/rough.dds", nmap2->Resource, nmap2->UploadHeap));
	mNormalMap[1] = std::move(nmap2);

	auto cmap = std::make_unique<Texture>();
	ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), L"res/textures/grasscube1024.dds", cmap->Resource, cmap->UploadHeap));
	mCubemap = std::move(cmap);
}

void App::buildDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 12;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mHeap->GetCPUDescriptorHandleForHeapStart());
	allocateOutputResources();
	hDescriptor.Offset(7, mCbvSrvUavDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.RaytracingAccelerationStructure.Location = mTopLevelASBuffers.pResult->GetGPUVirtualAddress();
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.MostDetailedMip = 0;

	//wood texture
	srvDesc.Format = mTexture->Resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mTexture->Resource->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(mTexture->Resource.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	//normal maps
	srvDesc.Format = mNormalMap[0]->Resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mNormalMap[0]->Resource->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(mNormalMap[0]->Resource.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.Format = mNormalMap[1]->Resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mNormalMap[1]->Resource->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(mNormalMap[1]->Resource.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0F;

	//cubemap
	srvDesc.Format = mCubemap->Resource->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mCubemap->Resource->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(mCubemap->Resource.Get(), &srvDesc, hDescriptor);
}

void App::allocateOutputResources()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mHeap->GetCPUDescriptorHandleForHeapStart());

	//output buffer and accumulation buffer
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	md3dDevice->CreateUnorderedAccessView(mOutputResource[0].Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mOutputResource[1].Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mNormalRoughness.Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mDepthBuffer.Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mMotionVectorBuffer.Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mZDepth.Get(), nullptr, &uavDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	md3dDevice->CreateUnorderedAccessView(mAlbedoMap.Get(), nullptr, &uavDesc, hDescriptor);
}

void App::buildFrameResources()
{
	for(int i = 0; i < NUM_FRAME_RESOURCES; ++i)
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), (UINT) mRTInstances.size(), (UINT) mMaterials.size()));
}

void App::update()
{
	keyboardInput();
	mouseInput();

	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	mFrameInExecution = mCurrFrameResource->fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->fence;

	if(mMainPassCB.frameIndex > 63)
		mMainPassCB.frameIndex = 1;
	
	if(!mFrameInExecution)
	{
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;

		if(settings.dlss || settings.RTAA > 1)
			jitter = { HaltonSequence(2, phase + 1) - 0.5F, HaltonSequence(3, phase + 1) - 0.5F };

		mCam->updateViewMatrix();

		updateMainPassCB();
		updateObjCBs();
		updateMaterialCBs();
		updateDenoiser();
	}
}

void App::draw()
{
	WaitForSingleObjectEx(mFrameWaitable, 1000, true);

	ThrowIfFailed(mCurrFrameResource->cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mCurrFrameResource->cmdListAlloc.Get(), nullptr));

	D3D12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(mOutputResource[0].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};
	mCommandList->ResourceBarrier(settings.dlss ? 1 : 2, barriers);

	ID3D12DescriptorHeap* heaps[] = { mHeap.Get() };
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

	desc.Width = settings.dlss ? settings.dlssWidth : settings.width;
	desc.Height = settings.dlss ? settings.dlssHeight : settings.height;
	desc.Depth = 1;

	mCommandList->SetPipelineState1(mRtStateObject.Get());
	mCommandList->DispatchRays(&desc);

	denoise(nrdSettings, mOutputResource[0].Get());
	if(settings.dlss)
		DLSS(mOutputResource[0].Get(), jitter.x, jitter.y);
	if(settings.dlss || settings.RTAA > 1)
		phase = (phase + 1) % phaseCount;

	if(settings.dlss)
	{
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mResolvedBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		mCommandList->ResourceBarrier(1, barriers);
		mCommandList->CopyResource(currentBackBuffer(), mResolvedBuffer.Get());
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mResolvedBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		mCommandList->ResourceBarrier(2, barriers);
	}
	else
	{
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mDenoisedTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputResource[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		mCommandList->ResourceBarrier(2, barriers);
		mCommandList->CopyResource(currentBackBuffer(), mDenoisedTexture.Get());
		barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
		barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mDenoisedTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		mCommandList->ResourceBarrier(2, barriers);
	}

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsList[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsList), cmdsList);

	ThrowIfFailed(mSwapChain->Present(settings.vSync && settings.fullscreen ? 1 : 0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;
	
	mCurrFrameResource->fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	mMainPassCB.frameIndex++;

	for(int i = 0; i < 4; ++i)
	{
		for(int j = 0; j < 4; ++j)
		{
			nrdSettings.viewToClipMatrixPrev[i * 4 + j] = mCam->getProj4x4()(i, j);
			nrdSettings.worldToViewMatrixPrev[i * 4 + j] = mCam->getView4x4()(i, j);
		}
	}
}

void App::updateMainPassCB()
{
	if(mCam->isDirty())
	{
		mCam->cleanView();

		XMMATRIX view = mCam->getView();

		XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));

		XMMATRIX proj = mCam->getProj();

		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMStoreFloat4x4(&mMainPassCB.viewProj, XMMatrixTranspose(viewProj));

		XMVECTOR viewDet = XMMatrixDeterminant(view);
		XMMATRIX invView = XMMatrixInverse(&viewDet, view);

		XMVECTOR projDet = XMMatrixDeterminant(proj);
		XMMATRIX invProj = XMMatrixInverse(&projDet, proj);

		XMStoreFloat4x4(&mMainPassCB.invView, invView);
		XMStoreFloat4x4(&mMainPassCB.invProj, invProj);

		mMainPassCB.fov = 0.25F * XM_PI;
		mMainPassCB.aspectRatio = aspectRatio();
		mMainPassCB.nearPlane = 0.01F;
		mMainPassCB.farPlane = 10000.0F;
		if(settings.dlss)
			mMainPassCB.LODOffset = log2f((float) settings.dlssWidth / settings.width) - 1.0F;
	}

	mMainPassCB.jitter = jitter;

	mCurrFrameResource->passCB->copyData(0, mMainPassCB);
}

void App::updateObjCBs()
{
	for(auto& e:mRTInstances)
	{
		if(e->numFramesDirty > 0)
		{
			ObjectConstants objCB;
			XMStoreFloat4x4(&objCB.world, XMMatrixTranspose(e->world));
			objCB.materialIndex = e->matOffset;
			objCB.diffuseIndex = e->texOffset;
			objCB.normalIndex = e->normalOffset;

			mCurrFrameResource->objCB->copyData(e->objCBOffset, objCB);
			e->numFramesDirty--;
		}
	}
}

void App::updateMaterialCBs()
{
	for(auto& m:mMaterials)
	{
		if(m->NumFramesDirty > 0)
		{
			MaterialConstants matCB;
			matCB.diffuseAlbedo = m->DiffuseAlbedo;
			matCB.fresnelR0 = m->FresnelR0;
			matCB.fresnelPower = m->fresnelPower;
			matCB.roughness = m->Roughness;
			matCB.metallic = m->metallic;
			matCB.refractionIndex = m->refractionIndex;
			matCB.flags = m->flags;

			mCurrFrameResource->matCB->copyData(m->matCBIndex, matCB);
			m->NumFramesDirty--;
		}
	}
}

void App::updateDenoiser()
{
	for(int i = 0; i < 4; ++i)
	{
		for(int j = 0; j < 4; ++j)
		{
			nrdSettings.viewToClipMatrix[i * 4 + j] = mCam->getProj4x4()(i, j);
			nrdSettings.worldToViewMatrix[i * 4 + j] = mCam->getView4x4()(i, j);
		}
	}

	nrdSettings.cameraJitter[0] = jitter.x;
	nrdSettings.cameraJitter[1] = jitter.y;
	nrdSettings.inputSubrectOrigin[0] = 0;
	nrdSettings.inputSubrectOrigin[1] = 0;
	nrdSettings.timeDeltaBetweenFrames = mTimer.deltaTime();
	nrdSettings.frameIndex = mMainPassCB.frameIndex;
	nrdSettings.disocclusionThreshold = 0.1F;
	nrdSettings.enableValidation = true;
	nrdSettings.splitScreen = 0.0F;
	nrdSettings.denoisingRange = 9000;
	nrdSettings.isMotionVectorInWorldSpace = false;
	//TODO dlss sizes
	nrdSettings.motionVectorScale[0] = 1.0F / settings.width;
	nrdSettings.motionVectorScale[1] = 1.0F / settings.height;
}

void App::onResize()
{
	Window::onResize();

	mCam->setLens(0.25F * XM_PI, aspectRatio(), 0.01F, 10000.0F);
	mMainPassCB.frameIndex = 1;

	if(mOutputResource[0])
	{
		if(settings.dlss)
			resetDLSSFeature();

		buildOutputResource();
		allocateOutputResources();
	}
}

void App::keyboardInput()
{
	const float dt = mTimer.deltaTime();
	const float speed = 5;

	if(keyboard.isKeyDown(KEY_W))
		mCam->walk(speed * dt);
	if(keyboard.isKeyDown(KEY_A))
		mCam->strafe(-speed * dt);
	if(keyboard.isKeyDown(KEY_S))
		mCam->walk(-speed * dt);
	if(keyboard.isKeyDown(KEY_D))
		mCam->strafe(speed * dt);

	if(keyboard.isKeyPressed(VK_F1))
	{
		settings.fps = 0;
		resetFPS();
	}

	if(keyboard.isKeyPressed(VK_ESCAPE))
	{
		toggleCursor(mCameraMode);
		mCameraMode = !mCameraMode;

		mouse.setPos(settings.width / 2, settings.height / 2);
		centerCursor();
	}

	if(keyboard.isKeyDown(VK_SPACE))
		mMainPassCB.frameIndex = 1;

	if(keyboard.isKeyPressed(VK_F11))
		toggleFullscreen();
}

void App::mouseInput()
{
	if(mCameraMode)
	{
		float sens = 20.0F;
		if(settings.vSync && settings.fullscreen) sens *= 0.5F;

		float dx = mTimer.deltaTime() * XMConvertToRadians(sens * (static_cast<float>(mouse.getMousePos().x) - (settings.width / 2)));
		float dy = mTimer.deltaTime() * XMConvertToRadians(sens * (static_cast<float>(mouse.getMousePos().y) - (settings.height / 2)));

		if(dx != 0 || dy != 0)
			centerCursor();
		if(dx != 0)
			mCam->rotateY(dx);
		if(dy != 0)
			mCam->pitch(dy);
	}
}