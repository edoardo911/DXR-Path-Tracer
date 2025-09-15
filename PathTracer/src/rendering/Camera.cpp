#include "Camera.h"

using namespace DirectX;

namespace RT
{
	void Camera::setLens(float fovY, float aspectRatio, float zn, float zf)
	{
		mFovY = fovY;
		mAspect = aspectRatio;
		mNearZ = zn;
		mFarZ = zf;

		mNearWindowHeight = 2.0F * mNearZ * tanf(0.5F * mFovY);
		mFarWindowHeight = 2.0F * mFarZ * tanf(0.5F * mFovY);

		XMMATRIX p = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
		XMStoreFloat4x4(&mProj, p);
		mViewDirty = true;
	}

	float Camera::getFovX() const
	{
		float halfWidth = 0.5F * getNearWindowWidth();
		return 2.0F * atan(halfWidth / mNearZ);
	}

	void Camera::walk(float dx)
	{
		XMVECTOR s = XMVectorReplicate(dx);
		XMVECTOR l = XMLoadFloat3(&mLook);
		XMVECTOR p = XMLoadFloat3(&mPosition);

		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
		mViewDirty = true;
	}

	void Camera::strafe(float dx)
	{
		XMVECTOR s = XMVectorReplicate(dx);
		XMVECTOR r = XMLoadFloat3(&mRight);
		XMVECTOR p = XMLoadFloat3(&mPosition);

		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
		mViewDirty = true;
	}

	void Camera::rotateY(float da)
	{
		XMMATRIX R = XMMatrixRotationY(da);

		XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
		mViewDirty = true;
	}

	void Camera::pitch(float da)
	{
		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), da);

		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
		mViewDirty = true;
	}

	void Camera::roll(float da)
	{
		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mLook), da);

		XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
		XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
		mViewDirty = true;
	}

	void Camera::setPos(float x, float y, float z)
	{
		mPosition = { x, y, z };
		mViewDirty = true;
	}

	void Camera::setPos(const XMFLOAT3& pos)
	{
		mPosition = pos;
		mViewDirty = true;
	}

	void Camera::setPos(const XMVECTOR& pos)
	{
		XMStoreFloat3(&mPosition, pos);
		mViewDirty = true;
	}

	void Camera::lookAt(XMVECTOR pos, XMVECTOR target, XMVECTOR worldUp)
	{
		XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
		XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
		XMVECTOR U = XMVector3Cross(L, R);

		XMStoreFloat3(&mPosition, pos);
		XMStoreFloat3(&mLook, L);
		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		mViewDirty = true;
	}

	void Camera::lookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& worldUp)
	{
		XMVECTOR P = XMLoadFloat3(&pos);
		XMVECTOR T = XMLoadFloat3(&target);
		XMVECTOR U = XMLoadFloat3(&worldUp);

		lookAt(P, T, U);
	}

	void Camera::saveState()
	{
		memcpy(mProjPrev.m, mProj.m, sizeof(float) * 16);
		memcpy(mViewPrev.m, mView.m, sizeof(float) * 16);
	}

	void Camera::updateViewMatrix()
	{
		if(mViewDirty)
		{
			XMVECTOR R = XMLoadFloat3(&mRight);
			XMVECTOR U = XMLoadFloat3(&mUp);
			XMVECTOR L = XMLoadFloat3(&mLook);
			XMVECTOR P = XMLoadFloat3(&mPosition);

			L = XMVector3Normalize(L);
			U = XMVector3Normalize(XMVector3Cross(L, R));
			R = XMVector3Cross(U, L);

			float x = -XMVectorGetX(XMVector3Dot(P, R));
			float y = -XMVectorGetX(XMVector3Dot(P, U));
			float z = -XMVectorGetX(XMVector3Dot(P, L));

			XMStoreFloat3(&mRight, R);
			XMStoreFloat3(&mUp, U);
			XMStoreFloat3(&mLook, L);

			mView(0, 0) = mRight.x;
			mView(1, 0) = mRight.y;
			mView(2, 0) = mRight.z;
			mView(3, 0) = x;

			mView(0, 1) = mUp.x;
			mView(1, 1) = mUp.y;
			mView(2, 1) = mUp.z;
			mView(3, 1) = y;

			mView(0, 2) = mLook.x;
			mView(1, 2) = mLook.y;
			mView(2, 2) = mLook.z;
			mView(3, 2) = z;

			mView(0, 3) = 0.0F;
			mView(1, 3) = 0.0F;
			mView(2, 3) = 0.0F;
			mView(3, 3) = 1.0F;
		}
	}
}