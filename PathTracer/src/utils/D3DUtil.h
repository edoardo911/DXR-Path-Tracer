#pragma once
#include "header.h"
#include "MathUtil.h"

namespace RT
{
    inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
    {
        if(obj)
            obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
    inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
    {
        if(obj)
            obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
    inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
    {
        if(obj)
            obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }

    inline std::wstring AnsiToWString(const std::string& str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
        return std::wstring(buffer);
    }

    struct Material
    {
        std::string Name;
        int NumFramesDirty = NUM_FRAME_RESOURCES;
        DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0F, 1.0F, 1.0F, 1.0F };
        DirectX::XMFLOAT3 FresnelR0 = { 0.1F, 0.1F, 0.1F };
        float fresnelPower = 0.0F;
        float Roughness = 0.1F;
        float metallic = 0.0F;
        float refractionIndex = 1.0F;
        UINT32 matCBIndex = 0;
        UINT32 flags = 0;
    };

    class D3DUtil
    {
    public:

        static bool IsKeyDown(int);

        inline static UINT CalcConstantBufferByteSize(UINT byteSize) { return (byteSize + 255) & ~255; }

        static Material loadMaterial(std::string);

        static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring&);
        static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device*, ID3D12GraphicsCommandList*, const void*, UINT64, Microsoft::WRL::ComPtr<ID3D12Resource>&);
        static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring&, const D3D_SHADER_MACRO*, const std::string&, const std::string&);
    };

    class DxException
    {
    public:
        DxException() = default;
        DxException(HRESULT, const std::wstring&, const std::wstring&, int);

        virtual std::wstring ToString() const;

        HRESULT ErrorCode = S_OK;
        std::wstring FunctionName;
        std::wstring Filename;
        int LineNumber = -1;
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
        std::string Name;

        Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

        Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

        UINT VertexByteStride = 0;
        UINT VertexBufferByteSize = 0;
        DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
        UINT IndexBufferByteSize = 0;

        std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

        D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
        {
            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
            vbv.StrideInBytes = VertexByteStride;
            vbv.SizeInBytes = VertexBufferByteSize;

            return vbv;
        }

        D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
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

    struct Light
    {
        DirectX::XMFLOAT3 Strength = { 0.5F, 0.5F, 0.5F };
        float FalloffStart = 1.0F;
        DirectX::XMFLOAT3 Direction = { 0.0F, -1.0F, 0.0F };
        float FalloffEnd = 10.0F;
        DirectX::XMFLOAT3 Position = { 0.0F, 0.0F, 0.0F };
        float SpotPower = 64.0F;
    };

    #define MaxLights 16

    struct Texture
    {
        std::string Name;
        std::wstring Filename;
        Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
    };

    #ifndef ReleaseCom
        #define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
    #endif
};