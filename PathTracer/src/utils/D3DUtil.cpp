#include "D3DUtil.h"

namespace RT
{
    using Microsoft::WRL::ComPtr;

    DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber):
        ErrorCode(hr),
        FunctionName(functionName),
        Filename(filename),
        LineNumber(lineNumber)
    {}

    bool D3DUtil::IsKeyDown(int vkeyCode) { return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0; }

    ComPtr<ID3DBlob> D3DUtil::LoadBinary(const std::wstring& filename)
    {
        std::ifstream fin(filename, std::ios::binary);

        fin.seekg(0, std::ios_base::end);
        std::ifstream::pos_type size = (int) fin.tellg();
        fin.seekg(0, std::ios_base::beg);

        ComPtr<ID3DBlob> blob;
        ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

        fin.read((char*) blob->GetBufferPointer(), size);
        fin.close();

        return blob;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> D3DUtil::CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
    {
        ComPtr<ID3D12Resource> defaultBuffer;

        // Create the actual default buffer resource.
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
        ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

        // In order to copy CPU memory data into our default buffer, we need to create
        // an intermediate upload heap. 
        hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

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

    ComPtr<ID3DBlob> D3DUtil::CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target)
    {
        UINT compileFlags = 0;
        #if defined(DEBUG) || defined(_DEBUG)  
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #endif

        HRESULT hr = S_OK;

        ComPtr<ID3DBlob> byteCode = nullptr;
        ComPtr<ID3DBlob> errors;
        hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

        if(errors != nullptr)
            OutputDebugStringA((char*) errors->GetBufferPointer());
        ThrowIfFailed(hr);

        return byteCode;
    }

    std::wstring DxException::ToString()const
    {
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }
};