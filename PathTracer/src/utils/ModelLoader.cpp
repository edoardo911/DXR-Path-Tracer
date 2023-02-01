#include "ModelLoader.h"

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

			XMVECTOR v0 = XMLoadFloat3(&vertices[i0].pos);
			XMVECTOR v1 = XMLoadFloat3(&vertices[i1].pos);
			XMVECTOR v2 = XMLoadFloat3(&vertices[i2].pos);

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
		MeshData mesh;
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT3> normals;
		std::vector<XMFLOAT2> uvs;
		std::vector<XMFLOAT3> tangents;

		std::vector<std::string> tokenList;
		size_t loc = 0;
		UINT32 index = 0;

		std::ifstream fs(filePath);
		if(fs.is_open())
		{
			std::string line;
			while(std::getline(fs, line))
			{
				char c1 = line[0], c2 = line[1];
				line = line.substr(line.find(" ") + 1, line.size());
				if(c1 == 'v')
				{
					if(c2 == 't')
					{
						XMFLOAT2 uv;
						loc = line.find(" ");
						uv.x = std::stof(line.substr(0, loc));
						uv.y = std::stof(line.substr(loc + 1, line.size()));
						uvs.push_back(uv);
					}
					else if(c2 == 'n')
					{
						XMFLOAT3 normal;
						loc = line.find(" ");
						normal.x = std::stof(line.substr(0, loc));
						line = line.substr(loc + 1, line.size());
						loc = line.find(" ");
						normal.y = std::stof(line.substr(0, loc));
						normal.z = std::stof(line.substr(loc + 1, line.size()));
						normals.push_back(normal);
					}
					else
					{
						XMFLOAT3 pos;
						loc = line.find(" ");
						pos.x = std::stof(line.substr(0, loc));
						line = line.substr(loc + 1, line.size());
						loc = line.find(" ");
						pos.y = std::stof(line.substr(0, loc));
						pos.z = std::stof(line.substr(loc + 1, line.size()));
						positions.push_back(pos);
					}
				}
				else if(c1 == 'f')
				{
					for(int i = 0; i < 3; ++i)
					{
						std::string token;

						Vertex v;
						if(i == 2)
							token = line;
						else
						{
							token = line.substr(0, line.find(" "));
							line = line.substr(line.find(" ") + 1, line.size());
						}

						auto find = std::find(tokenList.begin(), tokenList.end(), token);
						if(find == tokenList.end())
						{
							tokenList.push_back(token);

							v.pos = positions[std::stoi(token.substr(0, token.find("/"))) - 1];
							token = token.substr(token.find("/") + 1, token.size());
							//v.uvs = uvs[std::stoi(token.substr(0, token.find("/"))) - 1];
							token = token.substr(token.find("/") + 1, token.size());
							v.normal = normals[std::stoi(token) - 1];

							mesh.vertices.push_back(v);
							mesh.indices32.push_back(index);
							index++;
						}
						else
							mesh.indices32.push_back((UINT32) (find - tokenList.begin()));
					}
				}
			}

			calcTangents(mesh.indices32, mesh.vertices);
		}
		else
			throw std::exception(std::string("The file " + filePath + " does not exist").c_str());

		return mesh;
	}
};