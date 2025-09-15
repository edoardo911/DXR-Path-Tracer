#pragma once

#include "../utils/header.h"

namespace RT
{
	enum class RenderLayer: int
	{
		Opaque = 0,
		Transparent,
		AlphaTested,
		Water,
		Count
	};

	enum InstanceType
	{
		INSTANCE_TYPE_NORMAL = 0,
		INSTANCE_TYPE_WATER
	};

	struct InstanceInfo
	{
		DirectX::XMFLOAT3 pos = { 0.0F, 0.0F, 0.0F };
		DirectX::XMFLOAT3 rot = { 0.0F, 0.0F, 0.0F };
		DirectX::XMFLOAT3 scale = { 1.0F, 1.0F, 1.0F };
		float texScale = 1.0F;
		float distance = 0.0F;
		bool culled = false;
	};

	class Entity
	{
		friend class Scene;
	public:
		Entity() = default;
		~Entity() = default;
		Entity(const Entity&) = delete;
		Entity& operator=(const Entity&) = delete;

		//instance setters
		inline void setIndexCount(UINT value) { indexCount = value; }
		inline void setInstanceCount(UINT count) { instanceCount = count; }
		inline void setMaxInstanceCount(UINT count) { maxInstances = count; }
		inline void cleanOne() { numFramesDirty--; }
		void setPos(UINT id, DirectX::XMFLOAT3 pos);
		void setRotation(UINT id, DirectX::XMFLOAT3 rot);
		void setScale(UINT id, DirectX::XMFLOAT3 scale);
		void rotateX(UINT id, float amount);
		void rotateY(UINT id, float amount);
		void rotateZ(UINT id, float amount);

		inline void setGeo(MeshGeometry* geometry, std::string drawArgs = "0", UINT startIndex = 0, UINT baseVertex = 0)
		{
			geo = geometry;
			indexCount = (UINT) geo->DrawArgs[drawArgs].IndexCount;
			startIndexLocation = startIndex;
			baseVertexLocation = baseVertexLocation;
		}

		//getters/setters
		constexpr D3D12_PRIMITIVE_TOPOLOGY getPrimitiveTopology() const { return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; }

		inline std::vector<ObjectCB>& getInstances() { return instances; }
		inline std::vector<InstanceInfo>& getInstancesInfos() { return instancesInfo; }
		inline DirectX::BoundingBox& getBounds() { return bounds; }
		inline MeshGeometry* getGeo() const { return geo; }
		inline InstanceType getType() const { return type; }
		inline UINT getInstanceCount() const { return instanceCount; }
		inline UINT getIndex() const { return index; }
		inline UINT getIndexCount() const { return indexCount; }
		inline UINT getMaxInstances() const { return maxInstances; }
		inline UINT getStartIndex() const { return startIndexLocation; }
		inline UINT getBaseVertex() const { return baseVertexLocation; }
		inline bool isDirty() const { return numFramesDirty > 0; }
		inline void dirt() { numFramesDirty = NUM_FRAME_RESOURCES; }
		inline bool needsRefit() const { return refit; }
		inline RenderLayer getLayer() const { return layer; }
		inline float getDistance(UINT index) const { return instancesInfo[index].distance; }
		inline void setDistance(UINT index, float value) { instancesInfo[index].distance = value; }
		inline void setIndex(int index) { this->index = index; }
		inline INT32 getGeoIndex() const { return geoIndex; }

		inline void refitted() { refit = false; }
		inline void forceRefit() { refit = true; }
		inline void setInAction() { mAction = true; }

		inline bool isInAction()
		{
			bool value = mAction;
			mAction = false;
			return value;
		}

		//info
		inline DirectX::XMFLOAT3 getPos(UINT index) const { return instancesInfo[index].pos; }
		inline DirectX::XMFLOAT3 getRotation(UINT index) const { return instancesInfo[index].rot; }
		inline DirectX::XMFLOAT3 getScale(UINT index) const { return instancesInfo[index].scale; }
		inline bool isCulled(UINT index) const { return instancesInfo[index].culled; }
		inline void setCulled(UINT index, bool value) { instancesInfo[index].culled = value; }

		void scale(UINT index, float scale);
		void reloadWorld(UINT index);
		void addNewDefaultInstance();
		void saveState();
		void setLookingDirection(UINT id, DirectX::XMFLOAT3 dir);
	protected:
		void reloadLookingDirection(UINT id);

		const DirectX::XMFLOAT3 mUp = { 0.0F, 1.0F, 0.0F };
		DirectX::XMFLOAT3 lookingDirection = { 0.0F, 0.0F, 1.0F };

		MeshGeometry* geo = nullptr;
		DirectX::BoundingBox bounds;

		std::vector<ObjectCB> instances;
		std::vector<InstanceInfo> instancesInfo;
		UINT instanceCount = 0;
		UINT maxInstances = 0;

		UINT indexCount = 0;
		UINT startIndexLocation = 0;
		int baseVertexLocation = 0;
		InstanceType type = INSTANCE_TYPE_NORMAL;

		bool saveWorld = true;
		bool refit = false;
		bool mAction = false;

		UINT index = 0;
		UINT numFramesDirty = NUM_FRAME_RESOURCES;
		INT32 geoIndex = -1;
		RenderLayer layer = RenderLayer::Opaque;
	};
}