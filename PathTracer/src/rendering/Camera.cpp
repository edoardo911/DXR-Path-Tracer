#include "Camera.h"

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

		DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveFovLH(mFovY, aspectRatio, mNearZ, mFarZ);
		DirectX::XMStoreFloat4x4(&mProj, p);
		mViewDirty = true;
	}

	float Camera::getFovX() const
	{
		float halfWidth = 0.5F * getNearWindowWidth();
		return 2.0F * atan(halfWidth / mNearZ);
	}

	void Camera::walk(float d)
	{
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(d);
		DirectX::XMVECTOR l = DirectX::XMLoadFloat3(&mLook);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&mPosition);

		DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(s, l, p));
		mViewDirty = true;
	}

	void Camera::strafe(float d)
	{
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(d);
		DirectX::XMVECTOR r = DirectX::XMLoadFloat3(&mRight);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&mPosition);

		DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(s, r, p));
		mViewDirty = true;
	}

	void Camera::rotateY(float angle)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(angle);

		DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
		DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
		DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

		mViewDirty = true;
	}

	void Camera::pitch(float angle)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mRight), angle);

		DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
		DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

		mViewDirty = true;
	}

	void Camera::roll(float angle)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mLook), angle);

		DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
		DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));

		mViewDirty = true;
	}

	void Camera::setPos(float x, float y, float z)
	{
		mPosition = DirectX::XMFLOAT3(x, y, z);
		mViewDirty = true;
	}

	void Camera::setPos(const DirectX::XMFLOAT3& v)
	{
		mPosition = v;
		mViewDirty = true;
	}

	void Camera::lookAt(DirectX::XMVECTOR pos, DirectX::XMVECTOR target, DirectX::XMVECTOR worldUp)
	{
		DirectX::XMVECTOR L = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, pos));
		DirectX::XMVECTOR R = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, L));
		DirectX::XMVECTOR U = DirectX::XMVector3Cross(L, R);

		DirectX::XMStoreFloat3(&mPosition, pos);
		DirectX::XMStoreFloat3(&mLook, L);
		DirectX::XMStoreFloat3(&mRight, R);
		DirectX::XMStoreFloat3(&mUp, U);

		mViewDirty = true;
	}

	void Camera::lookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp)
	{
		DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&pos);
		DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&target);
		DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&worldUp);

		lookAt(P, T, U);
	}

	void Camera::updateViewMatrix()
	{
		if(mViewDirty)
		{
			DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&mRight);
			DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&mUp);
			DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&mLook);
			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&mPosition);

			L = DirectX::XMVector3Normalize(L);
			U = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(L, R));
			R = DirectX::XMVector3Cross(U, L);

			float x = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, R));
			float y = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, U));
			float z = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(P, L));

			DirectX::XMStoreFloat3(&mRight, R);
			DirectX::XMStoreFloat3(&mUp, U);
			DirectX::XMStoreFloat3(&mLook, L);

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