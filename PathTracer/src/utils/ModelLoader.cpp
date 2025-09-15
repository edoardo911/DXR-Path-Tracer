#include "ModelLoader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <queue>

using namespace DirectX;

namespace RT
{
	void ModelLoader::calcTangents(const std::vector<UINT32>& indices, std::vector<Vertex>& vertices)
	{
		for(int i = 0; i < indices.size() - 3; i += 3)
		{
			int i0 = indices[i];
			int i1 = indices[i + 1];
			int i2 = indices[i + 2];

			XMVECTOR v0 = XMLoadFloat3(&vertices[i0].position);
			XMVECTOR v1 = XMLoadFloat3(&vertices[i1].position);
			XMVECTOR v2 = XMLoadFloat3(&vertices[i2].position);

			XMVECTOR uv0 = XMLoadFloat2(&vertices[i0].uvs);
			XMVECTOR uv1 = XMLoadFloat2(&vertices[i1].uvs);
			XMVECTOR uv2 = XMLoadFloat2(&vertices[i2].uvs);

			XMVECTOR deltaPos1 = v1 - v0;
			XMVECTOR deltaPos2 = v2 - v0;
			XMVECTOR deltaUV1 = uv1 - uv0;
			XMVECTOR deltaUV2 = uv2 - uv0;

			float r = 1.0F / (XMVectorGetX(deltaUV1) * XMVectorGetY(deltaUV2) - XMVectorGetY(deltaUV1) * XMVectorGetX(deltaUV2));
			XMVECTOR tangent = XMVector3Normalize((deltaPos1 * XMVectorGetY(deltaUV2) - deltaPos2 * XMVectorGetY(deltaUV1)) * r);
			XMStoreFloat3(&vertices[i0].tangent, tangent);
			XMStoreFloat3(&vertices[i1].tangent, tangent);
			XMStoreFloat3(&vertices[i2].tangent, tangent);
		}
	}

	ModelLoader::MeshData ModelLoader::loadOBJ(std::string filePath)
	{
		MeshData meshData;

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err, warn;

		bool success = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
		if(!success)
			throw Win32Exception("Failed to load gltf file " + filePath);
		if(!warn.empty())
			Logger::WARN.log(warn);

		for(const auto& mesh:model.meshes)
		{
			for(const auto& primitive:mesh.primitives)
			{
				//extract data
				const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
				const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
				const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

				const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
				const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
				const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];

				const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
				const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
				const tinygltf::Buffer& uvBuffer = model.buffers[uvView.buffer];

				const tinygltf::Accessor& tanAccessor = model.accessors[primitive.attributes.at("TANGENT")];
				const tinygltf::BufferView& tanView = model.bufferViews[tanAccessor.bufferView];
				const tinygltf::Buffer& tanBuffer = model.buffers[tanView.buffer];

				bool hasBoneWeights = primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end();
				bool hasBoneIndices = primitive.attributes.find("JOINTS_0") != primitive.attributes.end();
				const tinygltf::Accessor* boneWeightsAccessor = hasBoneWeights ? &model.accessors[primitive.attributes.at("WEIGHTS_0")] : nullptr;
				const tinygltf::Accessor* boneIndicesAccessor = hasBoneIndices ? &model.accessors[primitive.attributes.at("JOINTS_0")] : nullptr;

				const tinygltf::BufferView* boneWeightView = hasBoneWeights ? &model.bufferViews[boneWeightsAccessor->bufferView] : nullptr;
				const tinygltf::BufferView* boneIndicesView = hasBoneIndices ? &model.bufferViews[boneIndicesAccessor->bufferView] : nullptr;
				const tinygltf::Buffer* boneWeightBuffer = hasBoneWeights ? &model.buffers[boneWeightView->buffer] : nullptr;
				const tinygltf::Buffer* boneIndicesBuffer = hasBoneIndices ? &model.buffers[boneIndicesView->buffer] : nullptr;

				size_t vertexCount = posAccessor.count;
				for(size_t i = 0; i < vertexCount; ++i)
				{
					Vertex v = {};

					//positions
					const float* pos = reinterpret_cast<const float*>(posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset + i * sizeof(DirectX::XMFLOAT3));
					v.position = { pos[0], pos[1], pos[2] };

					//normals
					const float* normal = reinterpret_cast<const float*>(normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset + i * sizeof(DirectX::XMFLOAT3));
					v.normal = { normal[0], normal[1], normal[2] };

					//uvs
					const float* uv = reinterpret_cast<const float*>(uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset + i * sizeof(DirectX::XMFLOAT2));
					v.uvs = { uv[0], uv[1] };

					//tangents
					const float* tangent = reinterpret_cast<const float*>(tanBuffer.data.data() + tanView.byteOffset + tanAccessor.byteOffset + i * sizeof(DirectX::XMFLOAT3));
					v.tangent = { tangent[0], tangent[1], tangent[2] };

					meshData.vertices.push_back(v);
				}

				//indices
				const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
				const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
				const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

				size_t indexCount = indexAccessor.count;
				for(size_t i = 0; i < indexCount; ++i)
				{
					uint32_t index;
					if(indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
						index = *(reinterpret_cast<const uint16_t*>(indexBuffer.data.data() + indexView.byteOffset + indexAccessor.byteOffset + i * sizeof(uint16_t)));
					else if(indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
						index = *(reinterpret_cast<const uint32_t*>(indexBuffer.data.data() + indexView.byteOffset + indexAccessor.byteOffset + i * sizeof(uint32_t)));
					meshData.indices32.push_back(index);
				}
			}
		}

		int skinIndex = -1;
		for(const auto& node:model.nodes)
		{
			if(node.skin >= 0)
			{
				skinIndex = node.skin;
				break;
			}
		}

		return meshData;
	}
};