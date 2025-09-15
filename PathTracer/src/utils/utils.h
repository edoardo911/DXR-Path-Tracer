#pragma once

#define NUM_FRAME_RESOURCES 3

namespace RT
{
	inline DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0F, 0.0F, 0.0F, 0.0F,
			0.0F, 1.0F, 0.0F, 0.0F,
			0.0F, 0.0F, 1.0F, 0.0F,
			0.0F, 0.0F, 0.0F, 1.0F
		);
		return I;
	}

	//structs
	struct Texture
	{
		std::string Name;
		std::wstring Filename;
		Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
	};

	struct Material
	{
		std::string name;
		int MatCBIndex = -1;
		int NumFramesDirty = NUM_FRAME_RESOURCES;
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0F, 1.0F, 1.0F, 1.0F };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01F, 0.01F, 0.01F };
		float Roughness = 0.25F;
		DirectX::XMFLOAT4X4 MatTransform = Identity4x4();
		DirectX::XMFLOAT3 emission = { 0.0F, 0.0F, 0.0F };
		float metallic = 0.0F;
		float refractionIndex = 1.0F;
		float specular = 0.3F;
		bool castsShadows = true;
	};

    struct SubmeshGeometry
    {
        UINT IndexCount = 0;
        UINT StartIndexLocation = 0;
        INT BaseVertexLocation = 0;
        DirectX::BoundingBox bounds;
    };

    struct MeshGeometry
    {
        std::string name;

        Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPUCopy = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPUPrev = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

        UINT VertexByteStride = 0;
        UINT VertexBufferByteSize = 0;
        UINT vertexCount = 0;
        DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
        UINT IndexBufferByteSize = 0;

        bool isWater = false;
        bool needsRefit = false;

        std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

        D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
        {
            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
            vbv.StrideInBytes = VertexByteStride;
            vbv.SizeInBytes = VertexBufferByteSize;

            return vbv;
        }

        D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
        {
            D3D12_INDEX_BUFFER_VIEW ibv;
            ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
            ibv.Format = IndexFormat;
            ibv.SizeInBytes = IndexBufferByteSize;

            return ibv;
        }

        void DisposeUploaders()
        {
            VertexBufferUploader = nullptr;
            IndexBufferUploader = nullptr;
        }
    };

	//functions
	inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
	}

    inline constexpr UINT CalcConstantBufferByteSize(UINT byteSize) { return (byteSize + 255) & ~255; }

    inline Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename)
    {
        std::ifstream fin(filename, std::ios::binary);

        fin.seekg(0, std::ios_base::end);
        std::ifstream::pos_type size = (int) fin.tellg();
        fin.seekg(0, std::ios_base::beg);

        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

        fin.read((char*) blob->GetBufferPointer(), size);
        fin.close();

        return blob;
    }

    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
        bool allowUav = false)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

        // Create the actual default buffer resource.
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize, allowUav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);
        ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

        // In order to copy CPU memory data into our default buffer, we need to create
        // an intermediate upload heap. 
        hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
        ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = initData;
        subResourceData.RowPitch = byteSize;
        subResourceData.SlicePitch = subResourceData.RowPitch;

        auto t = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList->ResourceBarrier(1, &t);
        UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        t = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->ResourceBarrier(1, &t);

        return defaultBuffer;
    }

    inline Material loadMaterial(std::string fileName)
    {
        std::ifstream file(fileName);

        Material mat{};

        std::string line;
        while(std::getline(file, line))
        {
            std::string param = line.substr(0, line.find("="));
            std::string value = line.substr(line.find("=") + 1, line.length());

            if(param == "albedo")
            {
                std::string num = value.substr(value.find("(") + 1, value.find(",") - 1);
                value = value.substr(value.find(",") + 1, value.length());
                mat.DiffuseAlbedo.x = std::stof(num);
                num = value.substr(0, value.find(","));
                value = value.substr(value.find(",") + 1, value.length());
                mat.DiffuseAlbedo.y = std::stof(num);
                num = value.substr(0, value.find(","));
                value = value.substr(value.find(",") + 1, value.length());
                mat.DiffuseAlbedo.z = std::stof(num);
                num = value.substr(0, value.find(")"));
                mat.DiffuseAlbedo.w = std::stof(num);
            }
            else if(param == "fresnelR0")
            {
                std::string num = value.substr(value.find("(") + 1, value.find(",") - 1);
                value = value.substr(value.find(",") + 1, value.length());
                mat.FresnelR0.x = std::stof(num);
                num = value.substr(0, value.find(","));
                value = value.substr(value.find(",") + 1, value.length());
                mat.FresnelR0.y = std::stof(num);
                num = value.substr(0, value.find(")"));
                value = value.substr(value.find(",") + 1, value.length());
                mat.FresnelR0.z = std::stof(num);
            }
            else if(param == "roughness")
                mat.Roughness = std::stof(value);
            else if(param == "metallic")
                mat.metallic = std::stof(value);
            else if(param == "emission")
            {
                std::string num = value.substr(value.find("(") + 1, value.find(",") - 1);
                value = value.substr(value.find(",") + 1, value.length());
                mat.emission.x = std::stof(num);
                num = value.substr(0, value.find(","));
                value = value.substr(value.find(",") + 1, value.length());
                mat.emission.y = std::stof(num);
                num = value.substr(0, value.find(")"));
                value = value.substr(value.find(",") + 1, value.length());
                mat.emission.z = std::stof(num);
            }
            else if(param == "refraction_index")
                mat.refractionIndex = std::stof(value);
            else if(param == "specular")
                mat.specular = std::stof(value);
            else if(param == "casts_shadows")
                mat.castsShadows = value == "true";
        }

        file.close();
        return mat;
    }

    inline void saveMaterial(std::string fileName, Material* mat)
    {
        std::ofstream file(fileName);

        file << "albedo=(" << mat->DiffuseAlbedo.x << ", " << mat->DiffuseAlbedo.y << ", " << mat->DiffuseAlbedo.z << ", " << mat->DiffuseAlbedo.w << ")\n";
        file << "fresnelR0=(" << mat->FresnelR0.x << ", " << mat->FresnelR0.y << ", " << mat->FresnelR0.z << ")\n";
        file << "roughness=" << mat->Roughness << "\n";
        if(mat->metallic > 0.0F)
            file << "metallic=" << mat->metallic << "\n";
        if(mat->refractionIndex < 1.0F)
            file << "refraction_index=" << mat->refractionIndex << "\n";
        file << "specular=" << mat->specular << "\n";
        if(!mat->castsShadows)
            file << "casts_shadows=false\n";

        file.close();
    }

    inline float RandF() { return (float) (rand() / (float) RAND_MAX); }
    inline float RandF(float a, float b) { return a + RandF() * (b - a); }

    inline float haltonSequence(int base, int phase)
    {
        float f = 1.0F;
        float r = 0.0F;
        int i = phase;

        while(i > 0)
        {
            f /= base;
            r += f * (i % base);
            i = (int) ((float) i / base);
        }

        return r;
    }

    inline DirectX::XMFLOAT3 sphericalToCartesian(float rho, float phi, float theta)
    {
        float x, y, z;
        x = rho * sinf(theta) * cosf(phi);
        z = rho * sinf(theta) * sinf(phi);
        y = rho * cosf(theta);
        return { x, y, z };
    }

    inline DXGI_FORMAT denoiserToDX(nrd::Format format)
    {
        switch(format)
        {
        default:
        case nrd::Format::R8_UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case nrd::Format::R8_SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case nrd::Format::R8_UINT:
            return DXGI_FORMAT_R8_UINT;
        case nrd::Format::R8_SINT:
            return DXGI_FORMAT_R8_SINT;
        case nrd::Format::RG8_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case nrd::Format::RG8_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case nrd::Format::RG8_UINT:
            return DXGI_FORMAT_R8G8_UINT;
        case nrd::Format::RG8_SINT:
            return DXGI_FORMAT_R8G8_SINT;
        case nrd::Format::RGBA8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case nrd::Format::RGBA8_SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case nrd::Format::RGBA8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case nrd::Format::RGBA8_SINT:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case nrd::Format::R16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case nrd::Format::R16_SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case nrd::Format::R16_UINT:
            return DXGI_FORMAT_R16_UINT;
        case nrd::Format::R16_SINT:
            return DXGI_FORMAT_R16_SINT;
        case nrd::Format::R16_SFLOAT:
            return DXGI_FORMAT_R16_FLOAT;
        case nrd::Format::RG16_UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case nrd::Format::RG16_SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case nrd::Format::RG16_UINT:
            return DXGI_FORMAT_R16G16_UINT;
        case nrd::Format::RG16_SINT:
            return DXGI_FORMAT_R16G16_SINT;
        case nrd::Format::RG16_SFLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;
        case nrd::Format::RGBA16_UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case nrd::Format::RGBA16_SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case nrd::Format::RGBA16_UINT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case nrd::Format::RGBA16_SINT:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case nrd::Format::RGBA16_SFLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case nrd::Format::R32_UINT:
            return DXGI_FORMAT_R32_UINT;
        case nrd::Format::R32_SINT:
            return DXGI_FORMAT_R32_SINT;
        case nrd::Format::R32_SFLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case nrd::Format::RG32_UINT:
            return DXGI_FORMAT_R32G32_UINT;
        case nrd::Format::RG32_SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case nrd::Format::RG32_SFLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case nrd::Format::RGB32_UINT:
            return DXGI_FORMAT_R32G32B32_UINT;
        case nrd::Format::RGB32_SINT:
            return DXGI_FORMAT_R32G32B32_SINT;
        case nrd::Format::RGB32_SFLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case nrd::Format::RGBA32_UINT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case nrd::Format::RGBA32_SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case nrd::Format::RGBA32_SFLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case nrd::Format::R10_G10_B10_A2_UNORM:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case nrd::Format::R10_G10_B10_A2_UINT:
            return DXGI_FORMAT_R10G10B10A2_UINT;
        case nrd::Format::R11_G11_B10_UFLOAT:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case nrd::Format::R9_G9_B9_E5_UFLOAT:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        }
    }
}