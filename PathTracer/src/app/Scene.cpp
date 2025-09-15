#include "Scene.h"

#include "../utils/GeometryGenerator.h"
#include "../utils/TextureLoader.h"
#include "../utils/ModelLoader.h"

using namespace DirectX;

namespace RT
{
	Scene::Scene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, settings_struct* settings, const std::string& fileName):
		fileName(fileName), settings(settings)
	{
		mCbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		init(device, cmdList, fileName);
	}

	Scene::~Scene()
	{
		mEntities.clear();
	}

	void Scene::init(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::string& fileName, bool reload)
	{
		Logger::INFO.log("Initializing scene \"" + fileName + "\"...");

		std::ifstream file;
		if(reload)
			file.open(this->fileName);
		else
		{
			if(fileName == "")
			{
				reload = true;
				materials = {};
				textures = {};
				nmaps = {};
				rmaps = {};
				hmaps = {};
				aomaps = {};
				emimaps = {};
				mmaps = {};
				cubemap = "";
				geometries = {};
			}
			else
				file.open(fileName);
		}

		if(!reload)
		{
			std::string line;
			while(std::getline(file, line))
			{
				std::string param = line.substr(0, line.find("="));
				std::string value = line.substr(line.find("=") + 1, line.length());

				if(param == "id")
					id = std::stoi(value);
				else if(param == "cubemap")
					cubemap = value;
				else if(param == "textures")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						textures.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					textures.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "nmaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						nmaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					nmaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "rmaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						rmaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					rmaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "hmaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						hmaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					hmaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "aomaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						aomaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					aomaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "emimaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						emimaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					emimaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "mmaps")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						mmaps.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					mmaps.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "impostors")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						impostors.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					impostors.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "materials")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						materials.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					materials.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "geometries")
				{
					value = value.substr(value.find("{") + 1, value.length());

					while(value.find(",") != std::string::npos)
					{
						geometries.push_back(value.substr(0, value.find(",")));
						value = value.substr(value.find(",") + 1, value.length());
					}

					geometries.push_back(value.substr(0, value.find("}")));
				}
				else if(param == "lights")
					mLightCount = std::stoi(value);
				else if(param == "events")
					mEventCount = std::stoi(value);
				else if(param == "cameras")
					mCameraCount = std::stoi(value);
				else if(param == "#cameras" || param == "#events" || param == "#lights" || param == "#instances")
					break;
			}

			loadMaterials(materials);
			buildDescriptorHeap(device);
			loadTextures(device, cmdList, textures);
			loadNormalMaps(device, cmdList, nmaps);
			loadRoughnessMaps(device, cmdList, rmaps);
			loadHeightMaps(device, cmdList, hmaps);
			loadAOMaps(device, cmdList, aomaps);
			loadEmissiveMaps(device, cmdList, emimaps);
			loadMetallicMaps(device, cmdList, mmaps);
			loadCubemap(device, cmdList, cubemap);
			loadGeometries(device, cmdList, geometries);
			if(mCameraCount > 0)
				loadCameras(file);
			if(mLightCount > 0)
				loadLights(file);
			loadInstances(file);
		}
		else
		{
			mMaterials.clear();
			mTextures.clear();
			mNMaps.clear();
			mRMaps.clear();
			mHMaps.clear();
			mAOMaps.clear();
			mEmissiveMaps.clear();
			mMMaps.clear();
			mGeometries.clear();

			if(fileName != "")
			{
				std::string line = "";
				while(line != "#lights" && line != "#instances")
					std::getline(file, line);
			}

			loadMaterials(materials, true);
			buildDescriptorHeap(device);
			loadTextures(device, cmdList, textures);
			loadNormalMaps(device, cmdList, nmaps);
			loadRoughnessMaps(device, cmdList, rmaps);
			loadHeightMaps(device, cmdList, hmaps);
			loadAOMaps(device, cmdList, aomaps);
			loadEmissiveMaps(device, cmdList, emimaps);
			loadMetallicMaps(device, cmdList, mmaps);
			loadCubemap(device, cmdList, cubemap);
			loadGeometries(device, cmdList, geometries);

			for(auto& e:mEntities)
				e->setGeo(mGeometries[e->getGeoIndex()].get());
		}

		if(fileName != "")
			file.close();
	}

	void Scene::buildDescriptorHeap(ID3D12Device* device)
	{
		Logger::INFO.log("Building shader descriptor heap...");

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = RESERVED_SPACE + MAX_ADD_RES;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeap)));
	}

	void Scene::loadMaterials(const std::vector<std::string>& materials, bool reload)
	{
		for(int i = 0; i < materials.size(); ++i)
		{
			std::unique_ptr<Material> mat = std::make_unique<Material>(loadMaterial("res/materials/" + materials[i] + ".mat"));
			mat->name = materials[i];
			mat->NumFramesDirty = NUM_FRAME_RESOURCES;
			mMaterials.push_back(std::move(mat));
		}
	}

	void Scene::loadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& textures, bool reload)
	{
		if(textures.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) textures.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC3_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mTextureArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mTextureArray.Get(), &srvDesc, handle);

		for(int i = 0; i < textures.size(); ++i)
		{
			std::wstring fileName = L"res/textures/" + std::wstring(textures[i].begin(), textures[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = textures[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mTextureArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mTextures.push_back(std::move(texture));
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mTextureArray.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);
	}

	void Scene::loadNormalMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& nmaps, bool reload)
	{
		if(nmaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) nmaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC5_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mNMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mNMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < nmaps.size(); ++i)
		{
			std::wstring fileName = L"res/normal_maps/" + std::wstring(nmaps[i].begin(), nmaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = nmaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mNMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mNMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadRoughnessMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& rmaps, bool reload)
	{
		if(rmaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) rmaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC4_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mRMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mRMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < rmaps.size(); ++i)
		{
			std::wstring fileName = L"res/roughness_maps/" + std::wstring(rmaps[i].begin(), rmaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = rmaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mRMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mRMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadHeightMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& hmaps, bool reload)
	{
		if(hmaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) hmaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC4_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mHMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 3, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mHMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < hmaps.size(); ++i)
		{
			std::wstring fileName = L"res/height_maps/" + std::wstring(hmaps[i].begin(), hmaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = hmaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mHMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mHMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadAOMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& aomaps, bool reload)
	{
		if(aomaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) aomaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC4_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mAOMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 4, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mAOMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < aomaps.size(); ++i)
		{
			std::wstring fileName = L"res/ao_maps/" + std::wstring(aomaps[i].begin(), aomaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = aomaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mAOMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mAOMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadEmissiveMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& emimaps, bool reload)
	{
		if(emimaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) emimaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC4_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mEmissiveMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 5, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mEmissiveMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < emimaps.size(); ++i)
		{
			std::wstring fileName = L"res/emissive/" + std::wstring(emimaps[i].begin(), emimaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = emimaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mEmissiveMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mEmissiveMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadMetallicMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& mmaps, bool reload)
	{
		if(mmaps.size() == 0)
			return;

		int mipmaps = settings->mipmaps ? (12 - settings->texResolution) : 1;

		//create texture array
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.Height = (UINT) pow(2, 11 - settings->texResolution);
		texDesc.DepthOrArraySize = (UINT) mmaps.size();
		texDesc.MipLevels = mipmaps;
		texDesc.Format = DXGI_FORMAT_BC4_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mMMapArray));

		//allocate texture array
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), 6, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = texDesc.DepthOrArraySize;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0F;
		device->CreateShaderResourceView(mMMapArray.Get(), &srvDesc, handle);

		for(int i = 0; i < mmaps.size(); ++i)
		{
			std::wstring fileName = L"res/metallic_maps/" + std::wstring(mmaps[i].begin(), mmaps[i].end()) + L".dds";

			auto texture = std::make_unique<Texture>();
			texture->Name = mmaps[i];
			ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), texture->Resource, texture->UploadHeap));

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->Resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);

			for(int mip = settings->texResolution; mip < (settings->mipmaps ? 12 : (settings->texResolution + 1)); ++mip)
			{
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = mMMapArray.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = D3D12CalcSubresource(mip - settings->texResolution, i, 0, mipmaps, texDesc.DepthOrArraySize);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = texture->Resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = mip;

				cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			}

			mMMaps.push_back(std::move(texture));
		}
	}

	void Scene::loadCubemap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::string& cubemap, bool reload)
	{
		if(cubemap == "")
			return;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0F;

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mHeap->GetCPUDescriptorHandleForHeapStart(), CUBEMAP_OFFSET, mCbvSrvUavDescriptorSize);

		std::wstring fileName = L"res/cubemaps/" + std::wstring(cubemap.begin(), cubemap.end()) + L".dds";

		mCubemap = std::make_unique<Texture>();
		mCubemap->Name = cubemap;
		ThrowIfFailed(CreateDDSTextureFromFile12(device, cmdList, fileName.c_str(), mCubemap->Resource, mCubemap->UploadHeap));

		srvDesc.Format = mCubemap->Resource->GetDesc().Format;
		srvDesc.TextureCube.MipLevels = settings->mipmaps ? mCubemap->Resource->GetDesc().MipLevels : 1;
		device->CreateShaderResourceView(mCubemap->Resource.Get(), &srvDesc, handle);
	}

	void Scene::loadGeometries(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& geometries)
	{
		Logger::INFO.log("Loading geometries...");

		XMFLOAT3 vMinf3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 vMaxf3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for(int i = 0; i < geometries.size(); ++i)
		{
			XMVECTOR vMin = XMLoadFloat3(&vMinf3);
			XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

			auto geom = std::make_unique<MeshGeometry>();
			geom->name = geometries[i];

			GeometryGenerator::MeshData meshData;
			std::vector<Vertex> vertices;
			std::vector<UINT16> indices;
			std::vector<UINT32> indices32;
			bool water = false;

			std::string token = geometries[i];
			if(token.at(0) == '#')
			{
				GeometryGenerator geoGen;

				token = token.substr(1, token.length());
				if(token == "box")
					meshData = geoGen.createBox(0.5F, 0.5F, 0.5F, 0);
				else if(token == "hills")
					meshData = geoGen.createGrid(10.0F, 10.0F, 75, 75);
				else if(token == "plane")
					meshData = geoGen.createGrid(10.0F, 10.0F, 2, 2);
				else if(token == "sphere")
					meshData = geoGen.createSphere(0.5F, 50, 50);
				else if(token == "quad")
					meshData = geoGen.createQuad(-3.5F, 2.5F, 7.0F, 3.0F, 2.0F);
				else if(token == "grid")
					meshData = geoGen.createGrid(7.0F, 7.0F, 2, 2);
				else if(token == "water_body50")
				{
					meshData = geoGen.createGrid(20.0F, 20.0F, 50, 50);
					water = true;
				}
				else if(token == "water_body75")
				{
					meshData = geoGen.createGrid(20.0F, 20.0F, 75, 75);
					water = true;
				}
				else if(token == "water_body100")
				{
					meshData = geoGen.createGrid(20.0F, 20.0F, 100, 100);
					water = true;
				}
				else if(token == "water_body150")
				{
					meshData = geoGen.createGrid(20.0F, 20.0F, 150, 150);
					water = true;
				}

				vertices.resize(meshData.vertices.size());
				indices = meshData.getIndices16();
				indices32 = meshData.indices32;

				for(size_t i = 0; i < meshData.vertices.size(); ++i)
				{
					vertices[i].position = meshData.vertices[i].position;
					vertices[i].uvs = meshData.vertices[i].texC;
					vertices[i].tangent = meshData.vertices[i].tangentU;
					vertices[i].normal = meshData.vertices[i].normal;

					XMVECTOR p = XMLoadFloat3(&vertices[i].position);
					vMin = XMVectorMin(vMin, p);
					vMax = XMVectorMax(vMax, p);
				}
			}
			else
			{
				ModelLoader::MeshData model = ModelLoader::loadOBJ("res/models/" + geometries[i] + ".glb");

				vertices = model.vertices;
				indices = model.getIndices16();
				indices32 = model.indices32;

				for(auto& v:vertices)
				{
					XMVECTOR p = XMLoadFloat3(&v.position);
					vMin = XMVectorMin(vMin, p);
					vMax = XMVectorMax(vMax, p);
				}
			}

			geom->vertexCount = (UINT) vertices.size();

			UINT vbByteSize = (UINT) vertices.size() * sizeof(Vertex);
			UINT ibByteSize = (UINT) indices32.size() * sizeof(UINT32);

			ThrowIfFailed(D3DCreateBlob(vbByteSize, &geom->VertexBufferCPU));
			CopyMemory(geom->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

			ThrowIfFailed(D3DCreateBlob(ibByteSize, &geom->IndexBufferCPU));
			CopyMemory(geom->IndexBufferCPU->GetBufferPointer(), indices32.data(), ibByteSize);

			geom->VertexBufferGPU = CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, geom->VertexBufferUploader);
			geom->IndexBufferGPU = CreateDefaultBuffer(device, cmdList, indices32.data(), ibByteSize, geom->IndexBufferUploader);

			geom->VertexByteStride = sizeof(Vertex);
			geom->VertexBufferByteSize = vbByteSize;
			geom->IndexFormat = DXGI_FORMAT_R32_UINT;
			geom->IndexBufferByteSize = ibByteSize;
			geom->isWater = water;

			SubmeshGeometry submesh;
			submesh.IndexCount = (UINT) indices.size();
			submesh.StartIndexLocation = 0;
			submesh.BaseVertexLocation = 0;
			XMStoreFloat3(&submesh.bounds.Center, 0.5F * (vMin + vMax));
			XMStoreFloat3(&submesh.bounds.Extents, 0.5F * (vMax - vMin));

			geom->DrawArgs["0"] = submesh;
			mGeometries.push_back(std::move(geom));
		}
	}

	void Scene::loadCameras(std::ifstream& file)
	{
		Logger::INFO.log("Loading cameras...");
		mCameras.resize(mCameraCount);

		int i = 0;
		std::string line;
		while(std::getline(file, line))
		{
			if(i == mCameraCount)
				break;
			else if(line != "" && line.at(0) == '{')
			{
				XMFLOAT3 pos = { 0.0F, 0.0F, 0.0F };
				XMFLOAT3 look = { 0.0F, 0.0F, 0.0F };
				while(line != "" && line.at(0) != '}')
				{
					std::getline(file, line);

					std::string param = line.substr(0, line.find("="));
					if(param != "" && param.at(0) == '\t')
						param = line.substr(1, line.find("=") - 1);
					std::string value = line.substr(line.find("=") + 1, line.length());

					if(param == "pos")
					{
						std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
						value = value.substr(value.find(",") + 1, value.length());
						pos.x = std::stof(token);

						token = value.substr(0, value.find(","));
						value = value.substr(value.find(",") + 1, value.length());
						pos.y = std::stof(token);

						token = value.substr(0, value.find(")"));
						pos.z = std::stof(token);
					}
					else if(param == "look")
					{
						std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
						value = value.substr(value.find(",") + 1, value.length());
						look.x = std::stof(token);

						token = value.substr(0, value.find(","));
						value = value.substr(value.find(",") + 1, value.length());
						look.y = std::stof(token);

						token = value.substr(0, value.find(")"));
						look.z = std::stof(token);
					}
				}

				XMFLOAT3 target = { pos.x + look.x, pos.y + look.y, pos.z + look.z };
				auto cam = std::make_unique<Camera>(static_cast<float>(settings->width) / settings->height);
				cam->lookAt(pos, target, { 0.0F, 1.0F, 0.0F });
				mCameras[i++] = std::move(cam);
			}
		}
	}

	void Scene::loadLights(std::ifstream& file)
	{
		Logger::INFO.log("Loading lights...");

		mLights.resize(mLightCount);

		int i = 0;
		std::string line;
		while(std::getline(file, line))
		{
			if(i == mLightCount)
				break;
			else if(line != "" && line.at(0) == '{')
			{
				while(line != "" && line.at(0) != '}')
				{
					std::getline(file, line);

					std::string param = line.substr(0, line.find("="));
					if(param != "" && param.at(0) == '\t')
						param = line.substr(1, line.find("=") - 1);
					std::string value = line.substr(line.find("=") + 1, line.length());

					if(param == "direction")
					{
						float x, y, z;
						std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
						value = value.substr(value.find(",") + 1, value.length());
						x = std::stof(token);

						token = value.substr(0, value.find(","));
						value = value.substr(value.find(",") + 1, value.length());
						y = std::stof(token);

						token = value.substr(0, value.find(")"));
						z = std::stof(token);

						XMStoreFloat3(&mLights[i].Direction, XMVector3Normalize(XMVectorSet(x, y, z, 0.0F)));
					}
					else if(param == "strength")
					{
						float r, g, b;
						std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
						value = value.substr(value.find(",") + 1, value.length());
						r = std::stof(token);

						token = value.substr(0, value.find(","));
						value = value.substr(value.find(",") + 1, value.length());
						g = std::stof(token);

						token = value.substr(0, value.find(")"));
						b = std::stof(token);

						XMStoreFloat3(&mLights[i].Strength, XMVectorSet(r, g, b, 0.0F));
					}
					else if(param == "pos")
					{
						float x, y, z;
						std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
						value = value.substr(value.find(",") + 1, value.length());
						x = std::stof(token);

						token = value.substr(0, value.find(","));
						value = value.substr(value.find(",") + 1, value.length());
						y = std::stof(token);

						token = value.substr(0, value.find(")"));
						z = std::stof(token);

						XMStoreFloat3(&mLights[i].Position, XMVectorSet(x, y, z, 0.0F));
					}
					else if(param == "falloff_start")
						mLights[i].FalloffStart = std::stof(value);
					else if(param == "falloff_end")
						mLights[i].FalloffEnd = std::stof(value);
					else if(param == "spot_power")
						mLights[i].SpotPower = std::stof(value);
					else if(param == "radius")
						mLights[i].radius = std::stof(value);
					else if(param == "type")
					{
						if(value == "directional")
							mLights[i].lightType = LIGHT_TYPE_DIRECTIONAL;
						else if(value == "spot_light")
							mLights[i].lightType = LIGHT_TYPE_SPOTLIGHT;
						else if(value == "point_light")
							mLights[i].lightType = LIGHT_TYPE_POINTLIGHT;
					}
				}
				++i;
			}
		}
	}

	void Scene::loadInstances(std::ifstream& file)
	{
		Logger::INFO.log("Loading instances...");

		std::string line;
		while(std::getline(file, line))
		{
			if(line != "" && line.at(0) == '{')
			{
				auto instance = std::make_unique<Entity>();

				while(line != "" && line.at(0) != '}')
				{
					std::getline(file, line);

					std::string param = line.substr(0, line.find("=") - 1);
					std::string paramColon = line.substr(0, line.find(":") - 1);
					if(param != "" && param.at(0) == '\t')
						param = line.substr(1, line.find("=") - 1);
					if(paramColon != "" && paramColon.at(0) == '\t')
						paramColon = line.substr(1, line.find(":") - 1);
					std::string value = line.substr(line.find("=") + 1, line.length());
					std::string valueColon = line.substr(line.find(":") + 1, line.length());

					if(param == "id")
						instance->index = std::stoi(value);
					else if(param == "geometry")
					{
						instance->geoIndex = std::stoi(value);
						if(instance->geoIndex >= 0)
							instance->geo = mGeometries[instance->geoIndex].get();
						if(instance->geo)
						{
							instance->indexCount = instance->geo->DrawArgs["0"].IndexCount;
							instance->startIndexLocation = instance->geo->DrawArgs["0"].StartIndexLocation;
							instance->baseVertexLocation = instance->geo->DrawArgs["0"].BaseVertexLocation;
							instance->bounds = instance->geo->DrawArgs["0"].bounds;
						}
					}
					else if(param == "layer")
					{
						if(value == "opaque")
							instance->layer = RenderLayer::Opaque;
						else if(value == "alpha_tested")
							instance->layer = RenderLayer::AlphaTested;
						else if(value == "transparent")
							instance->layer = RenderLayer::Transparent;
						else if(value == "water")
							instance->layer = RenderLayer::Water;
					}
					else if(param == "instances")
					{
						instance->instanceCount = std::stoi(value);
						instance->maxInstances = instance->instanceCount;

						int i = 0;
						ObjectCB prevInstance;
						InstanceInfo prevInfo;
						while(i != instance->instanceCount)
						{
							std::getline(file, line);
							if(line != "" && line.at(0) == '\t')
								line = line.substr(1, line.length());
							if(line != "" && line.at(0) == '{')
							{
								++i;
								ObjectCB inst;
								InstanceInfo info{};
								float x = 0.0F, y = 0.0F, z = 0.0F;
								float sx = 1.0F, sy = 1.0F, sz = 1.0F;
								float rx = 0.0F, ry = 0.0F, rz = 0.0F;
								float texScale = 1.0F;
								UINT lods = 0;

								while(line != "" && line.at(0) != '}')
								{
									std::getline(file, line);

									std::string param = line.substr(0, line.find("="));
									if(param != "" && param.at(0) == '\t')
										param = param.substr(1, param.find("="));
									if(param != "" && param.at(0) == '\t')
										param = param.substr(1, param.find("="));
									std::string value = line.substr(line.find("=") + 1, line.length());

									if(line == "+prev" || line == "\t+prev" || line == "\t\t+prev")
									{
										inst = prevInstance;
										info = prevInfo;
									}
									else if(param == "pos")
									{
										std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
										value = value.substr(value.find(",") + 1, value.length());
										x = std::stof(token);

										token = value.substr(0, value.find(","));
										value = value.substr(value.find(",") + 1, value.length());
										y = std::stof(token);

										token = value.substr(0, value.find(")"));
										z = std::stof(token);

										info.pos = { info.pos.x + x, info.pos.y + y, info.pos.z + z };
									}
									else if(param == "scale")
									{
										std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
										value = value.substr(value.find(",") + 1, value.length());
										sx = std::stof(token);

										token = value.substr(0, value.find(","));
										value = value.substr(value.find(",") + 1, value.length());
										sy = std::stof(token);

										token = value.substr(0, value.find(")"));
										sz = std::stof(token);

										info.scale = { info.scale.x * sx, info.scale.y * sy, info.scale.z * sz };
									}
									else if(param == "rot")
									{
										std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
										value = value.substr(value.find(",") + 1, value.length());
										rx = std::stof(token);

										token = value.substr(0, value.find(","));
										value = value.substr(value.find(",") + 1, value.length());
										ry = std::stof(token);

										token = value.substr(0, value.find(")"));
										rz = std::stof(token);

										info.rot = { info.rot.x + rx, info.rot.y + ry, info.rot.z + rz };
									}
									else if(param == "tex_scale")
									{
										float x, y, z;

										std::string token = value.substr(value.find("(") + 1, value.find(",") - 1);
										value = value.substr(value.find(",") + 1, value.length());
										x = std::stof(token);

										token = value.substr(0, value.find(","));
										value = value.substr(value.find(",") + 1, value.length());
										y = std::stof(token);

										token = value.substr(0, value.find(")"));
										z = std::stof(token);

										texScale = x;
										XMStoreFloat4x4(&inst.texTransform, XMMatrixScaling(x, y, z));

										info.texScale *= texScale;
									}
									else if(param == "material")
									{
										inst.materialIndex = std::stoi(value);
										if(instance->layer == RenderLayer::Water)
										{
											instance->type = INSTANCE_TYPE_WATER;
											inst.isWater = 1;
										}
									}
									else if(param == "texture")
										inst.textureIndex = std::stoi(value);
									else if(param == "nmap")
										inst.normalIndex = std::stoi(value);
									else if(param == "rmap")
										inst.roughIndex = std::stoi(value);
									else if(param == "hmap")
										inst.heightIndex = std::stoi(value);
									else if(param == "aomap")
										inst.aoIndex = std::stoi(value);
									else if(param == "emap")
										inst.emissiveIndex = std::stoi(value);
									else if(param == "mmap")
										inst.metallicIndex = std::stoi(value);
								}

								instance->instances.push_back(inst);
								prevInstance = inst;
								prevInfo = info;

								instance->instancesInfo.push_back(info);
								instance->reloadWorld(i - 1);
							}
						}
					}
				}

				mEntityLayer[(int) instance->layer].push_back(instance.get());
				mEntities.push_back(std::move(instance));
			}
		}
	}

	void Scene::reloadMaterials()
	{
		for(int i = 0; i < mMaterials.size(); ++i)
		{
			mMaterials[i]->MatCBIndex = i;
			mMaterials[i]->NumFramesDirty = NUM_FRAME_RESOURCES;
		}
	}

	void Scene::reloadInstances()
	{
		for(auto& ri:mEntities)
			ri->numFramesDirty = NUM_FRAME_RESOURCES;
	}

	void Scene::resizeCameras()
	{
		for(auto& c:mCameras)
			c->setLens(0.25F * XM_PI, static_cast<float>(settings->width) / settings->height, 0.1F, 1000.0F);
	}

	void Scene::evictTextures(ID3D12Device* device, bool textures, bool nmaps, bool rmaps, bool hmap, bool aomap, bool mmap)
	{
		std::vector<ID3D12Pageable*> res;
		if(textures && mTextureArray)
			res.push_back(mTextureArray.Get());
		if(nmaps && mNMapArray)
			res.push_back(mNMapArray.Get());
		if(rmaps && mRMapArray)
			res.push_back(mRMapArray.Get());
		if(hmap && mHMapArray)
			res.push_back(mHMapArray.Get());
		if(aomap && mAOMapArray)
			res.push_back(mAOMapArray.Get());
		if(mmap && mMMapArray)
			res.push_back(mMMapArray.Get());
		if(res.size() > 0)
			device->Evict((UINT) res.size(), res.data());
	}

	void Scene::makeResidentTextures(ID3D12Device* device, bool textures, bool nmaps, bool rmaps, bool hmap, bool aomap, bool mmap)
	{
		std::vector<ID3D12Pageable*> res;
		if(textures && mTextureArray)
			res.push_back(mTextureArray.Get());
		if(nmaps && mNMapArray)
			res.push_back(mNMapArray.Get());
		if(rmaps && mRMapArray)
			res.push_back(mRMapArray.Get());
		if(hmap && mHMapArray)
			res.push_back(mHMapArray.Get());
		if(aomap && mAOMapArray)
			res.push_back(mAOMapArray.Get());
		if(mmap && mMMapArray)
			res.push_back(mMMapArray.Get());
		if(res.size() > 0)
			device->MakeResident((UINT) res.size(), res.data());
	}
}