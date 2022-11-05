#pragma once
#include "header.h"

namespace RT
{
	class GeometryGenerator
	{
	public:
		struct Vertex
		{
			Vertex() {}
			Vertex(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT3& n, const DirectX::XMFLOAT3 t, const DirectX::XMFLOAT2 uv):
				position(p), normal(n), tangentU(t), texC(uv) {}
			Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float tz, float u, float v):
				position(px, py, pz), normal(nx, ny, nz), tangentU(tx, ty, tz), texC(u, v) {}

			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT3 normal;
			DirectX::XMFLOAT3 tangentU;
			DirectX::XMFLOAT2 texC;
		};

		struct MeshData
		{
			std::vector<Vertex> vertices;
			std::vector<UINT32> indices32;

			std::vector<UINT16>& getIndices16()
			{
				if(mIndices16.empty())
				{
					mIndices16.resize(indices32.size());
					for(size_t i = 0; i < indices32.size(); ++i)
						mIndices16[i] = static_cast<UINT16>(indices32[i]);
				}

				return mIndices16;
			}
		private:
			std::vector<UINT16> mIndices16;
		};

		///<summary>
		/// Creates a box centered at the origin with the given dimensions, where each
		/// face has m rows and n columns of vertices.
		///</summary>
		MeshData createBox(float width, float height, float depth, UINT32 numSubdivisions);

		///<summary>
		/// Creates a sphere centered at the origin with the given radius.  The
		/// slices and stacks parameters control the degree of tessellation.
		///</summary>
		MeshData createSphere(float radius, UINT32 sliceCount, UINT32 stackCount);

		///<summary>
		/// Creates a geosphere centered at the origin with the given radius.  The
		/// depth controls the level of tessellation.
		///</summary>
		MeshData createGeosphere(float radius, UINT32 numSubdivisions);

		///<summary>
		/// Creates a cylinder parallel to the y-axis, and centered about the origin.  
		/// The bottom and top radius can vary to form various cone shapes rather than true
		// cylinders.  The slices and stacks parameters control the degree of tessellation.
		///</summary>
		MeshData createCylinder(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount);

		///<summary>
		/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
		/// at the origin with the specified width and depth.
		///</summary>
		MeshData createGrid(float width, float depth, UINT32 m, UINT32 n);

		///<summary>
		/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
		///</summary>
		MeshData createQuad(float x, float y, float w, float h, float depth);
	private:
		void subdivide(MeshData& meshData);
		Vertex midPoint(const Vertex& v0, const Vertex& v1);
		void buildCylinderTopCap(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount, MeshData& meshData);
		void buildCylinderBottomCap(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount, MeshData& meshData);
	};
};