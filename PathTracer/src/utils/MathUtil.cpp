#include "MathUtil.h"

namespace RT
{
	const float MathUtil::infinity = FLT_MAX;
	const float MathUtil::pi = 3.1415926535F;

	float MathUtil::AngleFromXY(float x, float y)
	{
		float theta = 0.0f;

		if(x >= 0.0f)
		{
			theta = atanf(y / x);
			if(theta < 0.0f)
				theta += 2.0f * pi;
		}
		else
			theta = atanf(y / x) + pi;

		return theta;
	}

	DirectX::XMVECTOR MathUtil::RandVec3()
	{
		DirectX::XMVECTOR v = DirectX::XMVectorSet(MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), 0.0f);
		return DirectX::XMVector3Normalize(v);
	}

	DirectX::XMVECTOR MathUtil::RandUnitVec3()
	{
		DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);

		while(true)
		{
			DirectX::XMVECTOR v = DirectX::XMVectorSet(MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), 0.0f);
			if(DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
				continue;
			return DirectX::XMVector3Normalize(v);
		}
	}

	DirectX::XMVECTOR MathUtil::RandUnitVec3Up()
	{
		DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);

		while(true)
		{
			DirectX::XMVECTOR v = DirectX::XMVectorSet(MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(0.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), 0.0f);
			if(DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
				continue;
			return DirectX::XMVector3Normalize(v);
		}
	}

	DirectX::XMVECTOR MathUtil::RandHemisphereUnitVec3(DirectX::XMVECTOR n)
	{
		DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		DirectX::XMVECTOR Zero = DirectX::XMVectorZero();

		while(true)
		{
			DirectX::XMVECTOR v = DirectX::XMVectorSet(MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), MathUtil::RandF(-1.0f, 1.0f), 0.0f);

			if(DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
				continue;
			if(DirectX::XMVector3Less(DirectX::XMVector3Dot(n, v), Zero))
				continue;

			return DirectX::XMVector3Normalize(v);
		}
	}
};