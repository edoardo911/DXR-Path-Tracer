#include "Entity.h"

using namespace DirectX;

namespace RT
{
	void Entity::scale(UINT index, float scale)
	{
		instancesInfo[index].scale = { scale, scale, scale };
		reloadWorld(index);
	}

	void Entity::reloadWorld(UINT index)
	{
		InstanceInfo& info = instancesInfo[index];
		XMMATRIX rot = XMMatrixRotationX(info.rot.x) * XMMatrixRotationY(info.rot.y) * XMMatrixRotationZ(info.rot.z);
		XMStoreFloat4x4(&instances[index].world, XMMatrixScaling(info.scale.x, info.scale.y, info.scale.z) * rot * XMMatrixTranslation(info.pos.x, info.pos.y, info.pos.z));
		saveWorld = true;
	}

	void Entity::addNewDefaultInstance()
	{
		instances.push_back({});
		instancesInfo.push_back({});
	}

	void Entity::saveState()
	{
		if(saveWorld)
		{
			for(auto& i:instances)
			{
				memcpy(i.prevWorld.m, i.world.m, sizeof(float) * 16);
				numFramesDirty = NUM_FRAME_RESOURCES;
			}
			saveWorld = false;
			refit = true;
		}
	}

	void Entity::setPos(UINT id, XMFLOAT3 pos)
	{
		instancesInfo[id].pos = pos;
		reloadWorld(id);
	}

	void Entity::setRotation(UINT id, XMFLOAT3 rot)
	{
		instancesInfo[id].rot = rot;
		reloadWorld(id);
		reloadLookingDirection(id);		
	}

	void Entity::setScale(UINT id, XMFLOAT3 scale)
	{
		instancesInfo[id].scale = scale;
		reloadWorld(id);
	}

	void Entity::setLookingDirection(UINT id, XMFLOAT3 dir)
	{
		XMVECTOR dirV = XMLoadFloat3(&dir);
		XMVECTOR look = XMLoadFloat3(&lookingDirection);
		float angleFloat = XMVectorGetX(XMVector3AngleBetweenVectors(dirV, look));
		XMVECTOR mix = XMVector3Dot(XMVector3Cross(dirV, look), XMLoadFloat3(&mUp));

		lookingDirection = dir;
		if(XMVectorGetX(mix) < 0)
			instancesInfo[id].rot.y += angleFloat;
		else
			instancesInfo[id].rot.y -= angleFloat;
		reloadWorld(id);
	}

	void Entity::reloadLookingDirection(UINT id)
	{
		XMVECTOR newLook = XMVector3TransformNormal(XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), XMLoadFloat4x4(&instances[id].world));
		XMStoreFloat3(&lookingDirection, XMVector3Normalize(newLook));
	}

	void Entity::rotateX(UINT id, float amount)
	{
		instancesInfo[id].rot.x += amount;
		reloadWorld(id);
		reloadLookingDirection(id);
	}

	void Entity::rotateY(UINT id, float amount)
	{
		instancesInfo[id].rot.y += amount;
		reloadWorld(id);
		reloadLookingDirection(id);
	}

	void Entity::rotateZ(UINT id, float amount)
	{
		instancesInfo[id].rot.z += amount;
		reloadWorld(id);
		reloadLookingDirection(id);
	}
}