#pragma once
#include "header.h"

namespace RT
{
	class MathUtil
	{
	public:
		inline static float RandF() { return (float) (rand()) / (float) RAND_MAX; }
		inline static float RandF(float a, float b) { return a + RandF() * (b - a); }
		inline static int Rand(int a, int b) { return a + rand() % ((b - a) + 1); }

		template<typename T>
		inline static T Min(const T& a, const T& b) { return a < b ? a : b; }

		template<typename T>
		inline static T Max(const T& a, const T& b) { return a > b ? a : b; }

		template<typename T>
		inline static T Lerp(const T& a, const T& b, float t) { return a + (b - a) * t; }

		template<typename T>
		inline static T Clamp(const T& x, const T& low, const T& high) { return x < low ? low : (x > high ? high : x); }

		static float AngleFromXY(float x, float y);

		inline static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
		{
			return DirectX::XMVectorSet(
				radius * sinf(phi) * cosf(theta),
				radius * cosf(phi),
				radius * sinf(phi) * sinf(theta),
				1.0F);
		}

		inline static DirectX::XMMATRIX InverseTranspose(DirectX::XMMATRIX M)
		{
			DirectX::XMMATRIX A = M;
			
			A.r[3] = DirectX::XMVectorSet(0.0F, 0.0F, 0.0F, 1.0F);
			DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);

			return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));

		}

		inline static DirectX::XMFLOAT4X4 Identity4x4()
		{
			static DirectX::XMFLOAT4X4 I(
				1.0F, 0.0F, 0.0F, 0.0F,
				0.0F, 1.0F, 0.0F, 0.0F,
				0.0F, 0.0F, 1.0F, 0.0F,
				0.0F, 0.0F, 0.0F, 1.0F);

			return I;
		}

		static DirectX::XMVECTOR RandVec3();
		static DirectX::XMVECTOR RandUnitVec3();
		static DirectX::XMVECTOR RandUnitVec3Up();
		static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

		static const float infinity;
		static const float pi;
	};
};