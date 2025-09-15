#pragma once

#include "Entity.h"
#include "../utils/header.h"
#include "../rendering/Camera.h"

#define TEXTURE_OFFSET		0
#define NORMAL_OFFSET		1
#define ROUGHNESS_OFFSET	2
#define HEIGHT_OFFSET		3
#define AO_OFFSET			4
#define EMISSIVE_OFFSET		5
#define METALLIC_OFFSET		6
#define CUBEMAP_OFFSET		7
#define RESERVED_SPACE		8
#define MAX_ADD_RES			25

namespace RT
{
	class Scene
	{
	public:
		Scene(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, settings_struct* settings, const std::string& fileName);
		~Scene();
		inline Scene(const Scene&) = delete;
		inline Scene& operator=(const Scene&) = delete;

		inline std::vector<std::unique_ptr<Entity>>& getAllEntities() { return mEntities; }
		inline std::list<Entity*>& getEntityLayer(RenderLayer layer) { return mEntityLayer[(int) layer]; }
		inline UINT getEventCount() const { return mEventCount; }

		inline Camera* getSelectedCamera() { return mCameras[mSelectedCamera].get(); }
		inline Camera* getMainCamera() { return mCameras[0].get(); }
		inline void switchCamera(UINT index) { mSelectedCamera = index; }
		inline UINT getSelectedCameraIndex() const { return mSelectedCamera; }
		void resizeCameras();

		inline std::vector<std::wstring> getImpostors()
		{
			std::vector<std::wstring> result;
			for(auto& i:impostors)
				result.push_back(std::wstring(i.begin(), i.end()));
			return result;
		}

		inline UINT getMaterialCount() const { return (UINT) mMaterials.size(); }
		inline UINT getEntityCount() const { return (UINT) mEntities.size(); }
		inline UINT getLightCount() const { return mLightCount; }
		inline UINT getID() const { return id; }

		inline std::vector<std::unique_ptr<MeshGeometry>>& getResidentGeometries() { return mGeometries; }

		inline Light getLight(UINT index) const
		{
			if(index < mLights.size())
				return mLights[index];
			return {};
		}

		inline Light* getLightPtr(UINT index) { return &mLights[index]; }

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getLastCPUHeapAddress(UINT index = 0) { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeap->GetCPUDescriptorHandleForHeapStart(), RESERVED_SPACE + index, mCbvSrvUavDescriptorSize); }
		inline CD3DX12_GPU_DESCRIPTOR_HANDLE getLastGPUHeapAddress(UINT index = 0) { return CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeap->GetGPUDescriptorHandleForHeapStart(), RESERVED_SPACE + index, mCbvSrvUavDescriptorSize); }

		inline ID3D12DescriptorHeap* getDescriptorHeap() const { return mHeap.Get(); }

		inline std::vector<std::unique_ptr<Material>>& getMaterials() { return mMaterials; }

		void reloadMaterials();
		void reloadInstances();

		void evictTextures(ID3D12Device* device, bool textures, bool nmaps, bool rmaps, bool hmap, bool aomap, bool mmap);
		void makeResidentTextures(ID3D12Device* device, bool textures, bool nmaps, bool rmaps, bool hmap, bool aomap, bool mmap);
	protected:
		std::vector<std::string> materials;
		std::vector<std::string> textures;
		std::vector<std::string> nmaps;
		std::vector<std::string> rmaps;
		std::vector<std::string> hmaps;
		std::vector<std::string> aomaps;
		std::vector<std::string> emimaps;
		std::vector<std::string> mmaps;
		std::vector<std::string> impostors;
		std::vector<std::string> geometries;
		std::string cubemap = "";

		UINT mLightCount = 0;
		std::vector<Light> mLights;

		UINT mEventCount = 0;

		UINT mSelectedCamera = 0;
		UINT mCameraCount = 0;
		std::vector<std::unique_ptr<Camera>> mCameras;

		void init(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::string& fileName, bool reload = false);

		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getTextureCPUHeapAddress(UINT index = 0) { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeap->GetCPUDescriptorHandleForHeapStart(), index, mCbvSrvUavDescriptorSize); }
		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getNMapCPUHeapAddress(UINT index = 0) { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeap->GetCPUDescriptorHandleForHeapStart(), 1 + index, mCbvSrvUavDescriptorSize); }
		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getRMapCPUHeapAddress() { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeap->GetCPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize); }
		inline CD3DX12_CPU_DESCRIPTOR_HANDLE getCubemapCPUHeapAddress() { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeap->GetCPUDescriptorHandleForHeapStart(), 3, mCbvSrvUavDescriptorSize); }
	private:
		void loadMaterials(const std::vector<std::string>& materials, bool reload = false);
		void buildDescriptorHeap(ID3D12Device* device);
		void loadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& textures, bool reload = false);
		void loadNormalMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& nmaps, bool reload = false);
		void loadRoughnessMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& rmaps, bool reload = false);
		void loadHeightMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& hmaps, bool reload = false);
		void loadAOMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& aomaps, bool reload = false);
		void loadEmissiveMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& emimaps, bool reload = false);
		void loadMetallicMaps(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& mmaps, bool reload = false);
		void loadCubemap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::string& cubemap, bool reload = false);
		void loadGeometries(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::vector<std::string>& geometries);
		void loadCameras(std::ifstream& file);
		void loadLights(std::ifstream& file);
		void loadInstances(std::ifstream& file);

		Microsoft::WRL::ComPtr<ID3D12Resource> mTextureArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mNMapArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mRMapArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mHMapArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mAOMapArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mMMapArray = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> mEmissiveMapArray = nullptr;

		std::vector<std::unique_ptr<Material>> mMaterials;
		std::vector<std::unique_ptr<Texture>> mTextures;
		std::vector<std::unique_ptr<Texture>> mNMaps;
		std::vector<std::unique_ptr<Texture>> mRMaps;
		std::vector<std::unique_ptr<Texture>> mHMaps;
		std::vector<std::unique_ptr<Texture>> mAOMaps;
		std::vector<std::unique_ptr<Texture>> mMMaps;
		std::vector<std::unique_ptr<Texture>> mEmissiveMaps;
		std::vector<std::unique_ptr<MeshGeometry>> mGeometries;
		std::unique_ptr<Texture> mCubemap = nullptr;

		std::vector<std::unique_ptr<Entity>> mEntities;
		std::list<Entity*> mEntityLayer[(int) RenderLayer::Count];

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
		UINT id = 0;

		UINT mCbvSrvUavDescriptorSize;
		settings_struct* settings;
		std::string fileName;
	};
}