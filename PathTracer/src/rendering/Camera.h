#pragma once

#include "../utils/header.h"

namespace RT
{
	class Camera
	{
		friend class ModuleRightBar;
	public:
		inline Camera(float aspectRatio) { setLens(0.25F * DirectX::XM_PI, aspectRatio, 0.1F, 1000.0F); }
		~Camera() = default;

		void setPos(float x, float y, float z);
		void setPos(const DirectX::XMFLOAT3& pos);
		void setPos(const DirectX::XMVECTOR& pos);

		inline DirectX::XMMATRIX getView() const { return DirectX::XMLoadFloat4x4(&mView); }
		inline DirectX::XMMATRIX getProj() const { return DirectX::XMLoadFloat4x4(&mProj); }
		inline DirectX::XMMATRIX getViewPrev() const { return DirectX::XMLoadFloat4x4(&mViewPrev); }
		inline DirectX::XMMATRIX getProjPrev() const { return DirectX::XMLoadFloat4x4(&mProjPrev); }
		inline DirectX::XMVECTOR getPos() const { return DirectX::XMLoadFloat3(&mPosition); }
		inline DirectX::XMVECTOR getRight() const { return DirectX::XMLoadFloat3(&mRight); }
		inline DirectX::XMVECTOR getUp() const { return DirectX::XMLoadFloat3(&mUp); }
		inline DirectX::XMVECTOR getLook() const { return DirectX::XMLoadFloat3(&mLook); }
		inline DirectX::XMFLOAT3 getPos3F() const { return mPosition; }
		inline DirectX::XMFLOAT3 getRight3F() const { return mRight; }
		inline DirectX::XMFLOAT3 getUp3F() const { return mUp; }
		inline DirectX::XMFLOAT3 getLook3F() const { return mLook; }

		inline void cleanView() { mViewDirty = false; }
		inline void dirtView() { mViewDirty = true; }
		void saveState();

		inline float getNearZ() const { return mNearZ; }
		inline float getFarZ() const { return mFarZ; }
		inline float getAspect() const { return mAspect; }
		inline float getFovY() const { return mFovY; }
		inline bool isDirty() const { return mViewDirty; }
		float getFovX() const;

		inline float getNearWindowWidth() const { return mAspect * mNearWindowHeight; }
		inline float getNearWindowHeight() const { return mNearWindowHeight; }
		inline float getFarWindowWidth() const { return mAspect * mFarWindowHeight; }
		inline float getFarWindowHeight() const { return mFarWindowHeight; }

		void setLens(float fovY, float aspectRatio, float zn, float zf);

		void lookAt(DirectX::XMVECTOR pos, DirectX::XMVECTOR target, DirectX::XMVECTOR worldUp);
		void lookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp);

		inline DirectX::XMFLOAT4X4 getView4x4() const { return mView; }
		inline DirectX::XMFLOAT4X4 getProj4x4() const { return mProj; }
		inline DirectX::XMFLOAT4X4 getView4x4Prev() const { return mViewPrev; }
		inline DirectX::XMFLOAT4X4 getProj4x4Prev() const { return mProjPrev; }

		void walk(float dx);
		void strafe(float dx);

		void pitch(float da);
		void rotateY(float da);
		void roll(float da);

		void updateViewMatrix();
	private:
		DirectX::XMFLOAT3 mPosition = { 0.0F, 0.0F, 0.0F };
		DirectX::XMFLOAT3 mRight = { 1.0F, 0.0F, 0.0F };
		DirectX::XMFLOAT3 mUp = { 0.0F, 1.0F, 0.0F };
		DirectX::XMFLOAT3 mLook = { 0.0F, 0.0F, 1.0F };

		float mNearZ = 0.0F;
		float mFarZ = 0.0F;
		float mAspect = 0.0F;
		float mFovY = 0.0F;
		float mNearWindowHeight = 0.0F;
		float mFarWindowHeight = 0.0F;

		bool mViewDirty = true;

		DirectX::XMFLOAT4X4 mView = Identity4x4();
		DirectX::XMFLOAT4X4 mProj = Identity4x4();
		DirectX::XMFLOAT4X4 mViewPrev = Identity4x4();
		DirectX::XMFLOAT4X4 mProjPrev = Identity4x4();
	};
}