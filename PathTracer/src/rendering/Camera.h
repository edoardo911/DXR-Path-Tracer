#pragma once

#include "../utils/MathUtil.h"

namespace RT
{
	class Camera
	{
	public:
		inline Camera(float aspectRatio) { setLens(0.25F * MathUtil::pi, aspectRatio, 1.0F, 1000.0F); }
		~Camera() = default;

		void setPos(float, float, float);
		void setPos(const DirectX::XMFLOAT3&);

		void lookAt(const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&);
		void lookAt(DirectX::XMVECTOR, DirectX::XMVECTOR, DirectX::XMVECTOR);

		inline DirectX::XMMATRIX getView() const { return DirectX::XMLoadFloat4x4(&mView); }
		inline DirectX::XMMATRIX getProj() const { return DirectX::XMLoadFloat4x4(&mProj); }
		inline DirectX::XMFLOAT4X4 getView4x4() const { return mView; }
		inline DirectX::XMFLOAT4X4 getProj4x4() const { return mProj; }

		inline DirectX::XMVECTOR getPos() const { return DirectX::XMLoadFloat3(&mPosition); }
		inline DirectX::XMVECTOR getRight() const { return DirectX::XMLoadFloat3(&mRight); }
		inline DirectX::XMVECTOR getUp() const { return DirectX::XMLoadFloat3(&mUp); }
		inline DirectX::XMVECTOR getLook() const { return DirectX::XMLoadFloat3(&mLook); }
		inline DirectX::XMFLOAT3 getPos3F() const { return mPosition; }
		inline DirectX::XMFLOAT3 getRight3F() const { return mRight; }
		inline DirectX::XMFLOAT3 getUp3F() const { return mUp; }
		inline DirectX::XMFLOAT3 getLook3F() const { return mLook; }

		inline void cleanView() { mViewDirty = false; }

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

		void setLens(float, float, float, float);

		void walk(float);
		void strafe(float);

		void pitch(float);
		void rotateY(float);
		void roll(float);

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

		DirectX::XMFLOAT4X4 mView = MathUtil::Identity4x4();
		DirectX::XMFLOAT4X4 mProj = MathUtil::Identity4x4();
	};
};