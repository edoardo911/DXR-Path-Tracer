#pragma once

namespace RT
{
	struct Light
	{
		DirectX::XMFLOAT3 Strength;
		float FalloffStart;
		DirectX::XMFLOAT3 Direction;
		float FalloffEnd;
		DirectX::XMFLOAT3 Position;
		float SpotPower;
		int lightType;
		float radius;
		DirectX::XMFLOAT2 pad;
	};

	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uvs;
		DirectX::XMFLOAT3 tangent;
	};

	struct PassConstants
	{
		DirectX::XMFLOAT4X4 view = Identity4x4();
		DirectX::XMFLOAT4X4 proj = Identity4x4();
		DirectX::XMFLOAT4X4 invView = Identity4x4();
		DirectX::XMFLOAT4X4 invProj = Identity4x4();
		DirectX::XMFLOAT4X4 viewProj = Identity4x4();
		DirectX::XMFLOAT4X4 viewProjPrev = Identity4x4();
		DirectX::XMFLOAT4X4 viewPrev = Identity4x4();
		DirectX::XMFLOAT4X4 shadowTransform = Identity4x4();
		DirectX::XMFLOAT4X4 viewProjTex = Identity4x4();
		DirectX::XMFLOAT3 eyePosW = { 0.0F, 0.0F, 0.0F };
		UINT textureFilter = TEX_FILTER_NEAREST;
		UINT frameIndex = 1;
		int lightsCount = 0;
		int texturing = 1;
		int normalMapping = 1;
		int roughnessMapping = 1;
		int heightMapping = 1;
		int AOMapping = 1;
		int specular = 0;
		int reflectionsRT = 0;
		int refractionsRT = 0;
		int rtao = 0;
		int indirect = 0;
		UINT rayReconstruction = 0;
		UINT mipmaps = 0;
		int shadowsRT = 0;
		int metallicMapping = 0;
		DirectX::XMFLOAT2 jitter = { 0, 0 };
		float fov = 0.0F;
		float aspectRatio = 1.0F;
		Light lights[MAX_LIGHTS];
	};

	struct ObjectCB
	{
		DirectX::XMFLOAT4X4 world = Identity4x4();
		DirectX::XMFLOAT4X4 prevWorld = Identity4x4();
		DirectX::XMFLOAT4X4 texTransform = Identity4x4();
		INT32 materialIndex = -1;
		INT32 textureIndex = -1;
		INT32 normalIndex = -1;
		INT32 roughIndex = -1;
		INT32 heightIndex = -1;
		INT32 aoIndex = -1;
		INT32 emissiveIndex = -1;
		INT32 metallicIndex = -1;
		INT32 isWater = 0;
	};

	struct MaterialConstants
	{
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0F, 1.0F, 1.0F, 1.0F };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01F, 0.01F, 0.01F };
		float Roughness = 0.25F;
		DirectX::XMFLOAT4X4 MatTransform = Identity4x4();
		DirectX::XMFLOAT3 emission = { 0.0F, 0.0F, 0.0F };
		float metallic = 0.0F;
		float refractionIndex = 1.0F;
		float specular = 0.1F;
		int castsShadows = 1;
	};
}