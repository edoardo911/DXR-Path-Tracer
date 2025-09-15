#pragma once

#include "header.h"

namespace RT
{
	class ModelLoader
	{
	public:
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

		static MeshData loadOBJ(std::string);
		static void calcTangents(const std::vector<UINT32>& indices, std::vector<Vertex>& vertices);
	private:
		ModelLoader() = default;
	};
};