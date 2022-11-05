#include "GeometryGenerator.h"

namespace RT
{
	using namespace DirectX;

	GeometryGenerator::MeshData GeometryGenerator::createBox(float width, float height, float depth, UINT32 numSubdivisions)
	{
		MeshData meshData;

		//
		// Create the vertices.
		//

		Vertex v[24];

		float w2 = 0.5f * width;
		float h2 = 0.5f * height;
		float d2 = 0.5f * depth;

		// Fill in the front face vertex data.
		v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the back face vertex data.
		v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the top face vertex data.
		v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the bottom face vertex data.
		v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the left face vertex data.
		v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in the right face vertex data.
		v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		meshData.vertices.assign(&v[0], &v[24]);

		//
		// Create the indices.
		//

		UINT32 i[36];

		// Fill in the front face index data
		i[0] = 0; i[1] = 1; i[2] = 2;
		i[3] = 0; i[4] = 2; i[5] = 3;

		// Fill in the back face index data
		i[6] = 4; i[7] = 5; i[8] = 6;
		i[9] = 4; i[10] = 6; i[11] = 7;

		// Fill in the top face index data
		i[12] = 8; i[13] = 9; i[14] = 10;
		i[15] = 8; i[16] = 10; i[17] = 11;

		// Fill in the bottom face index data
		i[18] = 12; i[19] = 13; i[20] = 14;
		i[21] = 12; i[22] = 14; i[23] = 15;

		// Fill in the left face index data
		i[24] = 16; i[25] = 17; i[26] = 18;
		i[27] = 16; i[28] = 18; i[29] = 19;

		// Fill in the right face index data
		i[30] = 20; i[31] = 21; i[32] = 22;
		i[33] = 20; i[34] = 22; i[35] = 23;

		meshData.indices32.assign(&i[0], &i[36]);

		// Put a cap on the number of subdivisions.
		numSubdivisions = std::min<UINT32>(numSubdivisions, 6U);

		for(UINT32 i = 0; i < numSubdivisions; ++i)
			subdivide(meshData);

		return meshData;
	}

	void GeometryGenerator::subdivide(MeshData& meshData)
	{
		// Save a copy of the input geometry.
		MeshData inputCopy = meshData;

		meshData.vertices.resize(0);
		meshData.indices32.resize(0);

		//       v1
		//       *
		//      / \
		//     /   \
		//  m0*-----*m1
		//   / \   / \
		//  /   \ /   \
		// *-----*-----*
		// v0    m2     v2

		UINT32 numTris = (UINT32) inputCopy.indices32.size() / 3;
		for(UINT32 i = 0; i < numTris; ++i)
		{
			Vertex v0 = inputCopy.vertices[inputCopy.indices32[i * 3 + 0]];
			Vertex v1 = inputCopy.vertices[inputCopy.indices32[i * 3 + 1]];
			Vertex v2 = inputCopy.vertices[inputCopy.indices32[i * 3 + 2]];

			//
			// Generate the midpoints.
			//

			Vertex m0 = midPoint(v0, v1);
			Vertex m1 = midPoint(v1, v2);
			Vertex m2 = midPoint(v0, v2);

			//
			// Add new geometry.
			//

			meshData.vertices.push_back(v0); // 0
			meshData.vertices.push_back(v1); // 1
			meshData.vertices.push_back(v2); // 2
			meshData.vertices.push_back(m0); // 3
			meshData.vertices.push_back(m1); // 4
			meshData.vertices.push_back(m2); // 5

			meshData.indices32.push_back(i * 6 + 0);
			meshData.indices32.push_back(i * 6 + 3);
			meshData.indices32.push_back(i * 6 + 5);

			meshData.indices32.push_back(i * 6 + 3);
			meshData.indices32.push_back(i * 6 + 4);
			meshData.indices32.push_back(i * 6 + 5);

			meshData.indices32.push_back(i * 6 + 5);
			meshData.indices32.push_back(i * 6 + 4);
			meshData.indices32.push_back(i * 6 + 2);

			meshData.indices32.push_back(i * 6 + 3);
			meshData.indices32.push_back(i * 6 + 1);
			meshData.indices32.push_back(i * 6 + 4);
		}
	}
	
	GeometryGenerator::Vertex GeometryGenerator::midPoint(const Vertex& v0, const Vertex& v1)
	{
		XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.position);
		XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.position);

		XMVECTOR n0 = DirectX::XMLoadFloat3(&v0.normal);
		XMVECTOR n1 = DirectX::XMLoadFloat3(&v1.normal);

		XMVECTOR tan0 = DirectX::XMLoadFloat3(&v0.tangentU);
		XMVECTOR tan1 = DirectX::XMLoadFloat3(&v1.tangentU);

		XMVECTOR tex0 = DirectX::XMLoadFloat2(&v0.texC);
		XMVECTOR tex1 = DirectX::XMLoadFloat2(&v1.texC);

		// Compute the midpoints of all the attributes.  Vectors need to be normalized
		// since linear interpolating can make them not unit length.  
		XMVECTOR pos = 0.5F * (p0 + p1);
		XMVECTOR normal = XMVector3Normalize(0.5f * (n0 + n1));
		XMVECTOR tangent = XMVector3Normalize(0.5f * (tan0 + tan1));
		XMVECTOR tex = 0.5F * (tex0 + tex1);

		Vertex v;
		XMStoreFloat3(&v.position, pos);
		XMStoreFloat3(&v.normal, normal);
		XMStoreFloat3(&v.tangentU, tangent);
		XMStoreFloat2(&v.texC, tex);

		return v;
	}

	GeometryGenerator::MeshData GeometryGenerator::createCylinder(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount)
	{
		MeshData meshData;

		float stackHeight = height / stackCount;
		float radiusStep = (topRadius - bottomRadius) / stackCount;
		UINT32 ringCount = stackCount + 1;

		for(UINT32 i = 0; i < ringCount; ++i)
		{
			float y = -0.5F * height + i * stackHeight;
			float r = bottomRadius + i * radiusStep;

			float dTheta = 2.0F * XM_PI / sliceCount;
			for(UINT j = 0; j <= sliceCount; ++j)
			{
				Vertex vertex;

				float c = cosf(j * dTheta);
				float s = sinf(j * dTheta);

				vertex.position = XMFLOAT3(r * c, y, r * s);

				vertex.texC.x = (float) j / sliceCount;
				vertex.texC.y = 1.0F - (float) i / stackCount;

				vertex.tangentU = XMFLOAT3(-s, 0.0F, c);

				float dr = bottomRadius - topRadius;
				XMFLOAT3 bitangent = XMFLOAT3(dr * c, -height, dr * s);

				XMVECTOR T = XMLoadFloat3(&vertex.tangentU);
				XMVECTOR B = XMLoadFloat3(&bitangent);
				XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
				XMStoreFloat3(&vertex.normal, N);
				
				meshData.vertices.push_back(vertex);
			}
		}

		UINT32 ringVertexCount = sliceCount + 1;
		for(UINT32 i = 0; i < stackCount; ++i)
		{
			for(UINT32 j = 0; j < sliceCount; ++j)
			{
				meshData.indices32.push_back(i * ringVertexCount + j);
				meshData.indices32.push_back((i + 1) * ringVertexCount + j);
				meshData.indices32.push_back((i + 1) * ringVertexCount + j + 1);

				meshData.indices32.push_back(i * ringVertexCount + j);
				meshData.indices32.push_back((i + 1) * ringVertexCount + j + 1);
				meshData.indices32.push_back(i * ringVertexCount + j + 1);
			}
		}

		buildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
		buildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

		return meshData;
	}

	void GeometryGenerator::buildCylinderTopCap(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount, MeshData& meshData)
	{
		UINT32 baseIndex = (UINT32) meshData.vertices.size();

		float y = 0.5F * height;
		float dTheta = 2.0F * XM_PI / sliceCount;
		for(UINT32 i = 0; i <= sliceCount; ++i)
		{
			float x = topRadius * cosf(i * dTheta);
			float z = topRadius * sinf(i * dTheta);

			float u = x / height + 0.5F;
			float v = z / height + 0.5F;

			meshData.vertices.push_back(Vertex(x, y, z, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, u, v));
		}

		meshData.vertices.push_back(Vertex(0.0F, y, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.5F, 0.5F));

		UINT32 centerIndex = (UINT32) meshData.vertices.size() - 1;

		for(UINT32 i = 0; i < sliceCount; ++i)
		{
			meshData.indices32.push_back(centerIndex);
			meshData.indices32.push_back(baseIndex + i + 1);
			meshData.indices32.push_back(baseIndex + i);
		}
	}

	void GeometryGenerator::buildCylinderBottomCap(float bottomRadius, float topRadius, float height, UINT32 sliceCount, UINT32 stackCount, MeshData& meshData)
	{
		UINT32 baseIndex = (UINT32) meshData.vertices.size();
		float y = -0.5F * height;

		float dTheta = 2.0F * XM_PI / sliceCount;
		for(UINT32 i = 0; i <= sliceCount; ++i)
		{
			float x = bottomRadius * cosf(i * dTheta);
			float z = bottomRadius * sinf(i * dTheta);

			float u = x / height + 0.5F;
			float v = z / height + 0.5F;

			meshData.vertices.push_back(Vertex(x, y, z, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, u, v));
		}

		meshData.vertices.push_back(Vertex(0.0F, y, 0.0F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.5F, 0.5F));

		UINT32 centerIndex = (UINT32) meshData.vertices.size() - 1;

		for(UINT32 i = 0; i < sliceCount; ++i)
		{
			meshData.indices32.push_back(centerIndex);
			meshData.indices32.push_back(baseIndex + i);
			meshData.indices32.push_back(baseIndex + i + 1);
		}
	}

	GeometryGenerator::MeshData GeometryGenerator::createSphere(float radius, UINT32 sliceCount, UINT32 stackCount)
	{
		MeshData meshData;

		//
		// Compute the vertices stating at the top pole and moving down the stacks.
		//

		// Poles: note that there will be texture coordinate distortion as there is
		// not a unique point on the texture map to assign to the pole when mapping
		// a rectangular texture onto a sphere.
		Vertex topVertex(0.0F, +radius, 0.0F, 0.0F, +1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F);
		Vertex bottomVertex(0.0F, -radius, 0.0F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F);

		meshData.vertices.push_back(topVertex);

		float phiStep = XM_PI / stackCount;
		float thetaStep = 2.0F * XM_PI / sliceCount;

		// Compute vertices for each stack ring (do not count the poles as rings).
		for(UINT32 i = 1; i <= stackCount - 1; ++i)
		{
			float phi = i * phiStep;

			// Vertices of ring.
			for(UINT32 j = 0; j <= sliceCount; ++j)
			{
				float theta = j * thetaStep;

				Vertex v;

				// spherical to cartesian
				v.position.x = radius * sinf(phi) * cosf(theta);
				v.position.y = radius * cosf(phi);
				v.position.z = radius * sinf(phi) * sinf(theta);

				// Partial derivative of P with respect to theta
				v.tangentU.x = -radius * sinf(phi) * sinf(theta);
				v.tangentU.y = 0.0f;
				v.tangentU.z = +radius * sinf(phi) * cosf(theta);

				XMVECTOR T = XMLoadFloat3(&v.tangentU);
				XMStoreFloat3(&v.tangentU, XMVector3Normalize(T));

				XMVECTOR p = XMLoadFloat3(&v.position);
				XMStoreFloat3(&v.normal, XMVector3Normalize(p));

				v.texC.x = theta / XM_2PI;
				v.texC.y = phi / XM_PI;

				meshData.vertices.push_back(v);
			}
		}

		meshData.vertices.push_back(bottomVertex);

		//
		// Compute indices for top stack.  The top stack was written first to the vertex buffer
		// and connects the top pole to the first ring.
		//

		for(UINT32 i = 1; i <= sliceCount; ++i)
		{
			meshData.indices32.push_back(0);
			meshData.indices32.push_back(i + 1);
			meshData.indices32.push_back(i);
		}

		//
		// Compute indices for inner stacks (not connected to poles).
		//

		// Offset the indices to the index of the first vertex in the first ring.
		// This is just skipping the top pole vertex.
		UINT32 baseIndex = 1;
		UINT32 ringVertexCount = sliceCount + 1;
		for(UINT32 i = 0; i < stackCount - 2; ++i)
		{
			for(UINT32 j = 0; j < sliceCount; ++j)
			{
				meshData.indices32.push_back(baseIndex + i * ringVertexCount + j);
				meshData.indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

				meshData.indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
				meshData.indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
			}
		}

		//
		// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
		// and connects the bottom pole to the bottom ring.
		//

		// South pole vertex was added last.
		UINT32 southPoleIndex = (UINT32) meshData.vertices.size() - 1;

		// Offset the indices to the index of the first vertex in the last ring.
		baseIndex = southPoleIndex - ringVertexCount;

		for(UINT32 i = 0; i < sliceCount; ++i)
		{
			meshData.indices32.push_back(southPoleIndex);
			meshData.indices32.push_back(baseIndex + i);
			meshData.indices32.push_back(baseIndex + i + 1);
		}

		return meshData;
	}

	GeometryGenerator::MeshData GeometryGenerator::createGeosphere(float radius, UINT32 subdivisions)
	{
		MeshData meshData;

		subdivisions = std::min<UINT32>(subdivisions, 6U);

		const float x = 0.525731F;
		const float z = 0.850651F;

		XMFLOAT3 pos[12] = {
			XMFLOAT3(-x, 0.0F, z), XMFLOAT3(x, 0.0F, z),
			XMFLOAT3(-x, 0.0F, -z), XMFLOAT3(x, 0.0F, -z),
			XMFLOAT3(0.0F, z, x), XMFLOAT3(0.0F, z, -x),
			XMFLOAT3(0.0F, -z, x), XMFLOAT3(0.0F, -z, -x),
			XMFLOAT3(z, x, 0.0F), XMFLOAT3(-z, x, 0.0F),
			XMFLOAT3(z, -x, 0.0F), XMFLOAT3(-z, -x, 0.0F)
		};

		UINT32 k[60] = {
			1, 4, 0,   4, 9, 0,   4, 5, 9,   8, 5, 4,   1, 8, 4,
			1, 10, 8,  10, 3, 8,  8, 3, 5,   3, 2, 5,   3, 7, 2,
			3, 10, 7,  10, 6, 7,  6, 11, 7,  6, 0, 11,  6, 1, 0,
			10, 1, 6,  11, 0, 9,  2, 11, 9,  5, 2, 9,   11, 2, 7
		};

		meshData.vertices.resize(12);
		meshData.indices32.assign(&k[0], &k[60]);

		for(UINT32 i = 0; i < 12; ++i)
			meshData.vertices[i].position = pos[i];
		for(UINT32 i = 0; i < subdivisions; ++i)
			subdivide(meshData);

		for(UINT32 i = 0; i < meshData.vertices.size(); ++i)
		{
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&meshData.vertices[i].position));
			XMVECTOR p = radius * n;

			XMStoreFloat3(&meshData.vertices[i].position, p);
			XMStoreFloat3(&meshData.vertices[i].normal, n);

			float theta = atan2f(meshData.vertices[i].position.z, meshData.vertices[i].position.x);
			if(theta <= 0.0F)
				theta += XM_2PI;

			float phi = acosf(meshData.vertices[i].position.y / radius);
			
			meshData.vertices[i].texC.x = theta / XM_2PI;
			meshData.vertices[i].texC.y = phi / XM_PI;

			meshData.vertices[i].tangentU.x = -radius * sinf(phi) * sinf(theta);
			meshData.vertices[i].tangentU.y = 0.0F;
			meshData.vertices[i].tangentU.z = +radius * sinf(phi) * cosf(theta);

			XMVECTOR T = XMLoadFloat3(&meshData.vertices[i].tangentU);
			XMStoreFloat3(&meshData.vertices[i].tangentU, XMVector3Normalize(T));
		}

		return meshData;
	}

	GeometryGenerator::MeshData GeometryGenerator::createGrid(float width, float depth, UINT32 m, UINT32 n)
	{
		MeshData meshData;

		UINT32 vertexCount = m * n;
		UINT32 faceCount = (m - 1) * (n - 1) * 2;

		//
		// Create the vertices.
		//

		float halfWidth = 0.5F * width;
		float halfDepth = 0.5F * depth;

		float dx = width / (n - 1);
		float dz = depth / (m - 1);

		float du = 1.0F / (n - 1);
		float dv = 1.0F / (m - 1);

		meshData.vertices.resize(vertexCount);
		for(UINT32 i = 0; i < m; ++i)
		{
			float z = halfDepth - i * dz;
			for(UINT32 j = 0; j < n; ++j)
			{
				float x = -halfWidth + j * dx;

				meshData.vertices[i * n + j].position = XMFLOAT3(x, 0.0F, z);
				meshData.vertices[i * n + j].normal = XMFLOAT3(0.0F, 1.0F, 0.0F);
				meshData.vertices[i * n + j].tangentU = XMFLOAT3(1.0F, 0.0F, 0.0F);

				// Stretch texture over grid.
				meshData.vertices[i * n + j].texC.x = j * du;
				meshData.vertices[i * n + j].texC.y = i * dv;
			}
		}

		//
		// Create the indices.
		//

		meshData.indices32.resize(faceCount * 3); // 3 indices per face

		// Iterate over each quad and compute indices.
		UINT32 k = 0;
		for(UINT32 i = 0; i < m - 1; ++i)
		{
			for(UINT32 j = 0; j < n - 1; ++j)
			{
				meshData.indices32[k] = i * n + j;
				meshData.indices32[k + 1] = i * n + j + 1;
				meshData.indices32[k + 2] = (i + 1) * n + j;

				meshData.indices32[k + 3] = (i + 1) * n + j;
				meshData.indices32[k + 4] = i * n + j + 1;
				meshData.indices32[k + 5] = (i + 1) * n + j + 1;

				k += 6; // next quad
			}
		}

		return meshData;
	}

	GeometryGenerator::MeshData GeometryGenerator::createQuad(float x, float y, float w, float h, float depth)
	{
		MeshData meshData;

		meshData.vertices.resize(4);
		meshData.indices32.resize(6);

		// Position coordinates specified in NDC space.
		meshData.vertices[0] = Vertex(
			x, y - h, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 1.0f);

		meshData.vertices[1] = Vertex(
			x, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 0.0f);

		meshData.vertices[2] = Vertex(
			x + w, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f);

		meshData.vertices[3] = Vertex(
			x + w, y - h, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 1.0f);

		meshData.indices32[0] = 0;
		meshData.indices32[1] = 1;
		meshData.indices32[2] = 2;

		meshData.indices32[3] = 0;
		meshData.indices32[4] = 2;
		meshData.indices32[5] = 3;

		return meshData;
	}
};

