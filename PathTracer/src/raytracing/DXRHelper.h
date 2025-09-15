/******************************************************************************
 * Copyright 1998-2018 NVIDIA Corp. All Rights Reserved.
 *****************************************************************************/

#pragma once

#include "../utils/header.h"
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

#include <vector>

namespace nv_helpers_dx12
{
	//--------------------------------------------------------------------------------------------------
	//
	//
	inline void CreateBuffer(ID3D12Device* m_device, const uint64_t size,
	                                    const D3D12_RESOURCE_FLAGS flags, const D3D12_RESOURCE_STATES initState,
	                                    const D3D12_HEAP_PROPERTIES& heapProps, Microsoft::WRL::ComPtr<ID3D12Resource>& pBuffer)
	{
		D3D12_RESOURCE_DESC bufDesc;
		bufDesc.Alignment = 0;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Flags = flags;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.Height = 1;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufDesc.MipLevels = 1;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.SampleDesc.Quality = 0;
		bufDesc.Width = size;

		ThrowIfFailed(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer)));
	}

	// Specifies a heap used for uploading. This heap type has CPU access optimized
	// for uploading to the GPU.
	static const D3D12_HEAP_PROPERTIES kUploadHeapProps = {
		D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0
	};

	// Specifies the default heap. This heap type experiences the most bandwidth for
	// the GPU, but cannot provide CPU access.
	static const D3D12_HEAP_PROPERTIES kDefaultHeapProps = {
		D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0
	};

	//--------------------------------------------------------------------------------------------------
	// Compile a HLSL file into a DXIL library
	//
	inline IDxcBlob* CompileShaderLibrary(LPCWSTR fileName)
	{
		static IDxcCompiler* pCompiler = nullptr;
		static IDxcLibrary* pLibrary = nullptr;
		static IDxcIncludeHandler* dxcIncludeHandler;

		HRESULT hr;

		// Initialize the DXC compiler and compiler helper
		if (!pCompiler)
		{
			ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), reinterpret_cast<void**>(&pCompiler)));
			ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), reinterpret_cast<void**>(&pLibrary)));
			ThrowIfFailed(pLibrary->CreateIncludeHandler(&dxcIncludeHandler));
		}
		// Open and read the file
		std::ifstream shaderFile(fileName);
		if(shaderFile.good() == false)
			throw RT::RaytracingException("Cannot find shader file");
		std::stringstream strStream;
		strStream << shaderFile.rdbuf();
		std::string sShader = strStream.str();

		// Create blob from the string
		IDxcBlobEncoding* pTextBlob;
		ThrowIfFailed(pLibrary->CreateBlobWithEncodingFromPinned(LPBYTE(sShader.c_str()), static_cast<uint32_t>(sShader.size()), 0, &pTextBlob));

		// Compile
		IDxcOperationResult* pResult;
		ThrowIfFailed(pCompiler->Compile(pTextBlob, fileName, L"", L"lib_6_3", nullptr, 0, nullptr, 0, dxcIncludeHandler, &pResult));

		// Verify the result
		HRESULT resultCode;
		ThrowIfFailed(pResult->GetStatus(&resultCode));
		if(FAILED(resultCode))
		{
			IDxcBlobEncoding* pError;
			hr = pResult->GetErrorBuffer(&pError);
			if(FAILED(hr))
				throw RT::RaytracingException("Failed to get shader compiler error");

			// Convert error blob to a string
			std::vector<char> infoLog(pError->GetBufferSize() + 1);
			memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
			infoLog[pError->GetBufferSize()] = 0;

			std::string errorMsg = "Shader Compiler Error:\n";
			errorMsg.append(infoLog.data());
			
			std::string error = "Failed compile shader\n\n" + errorMsg;
			throw RT::RaytracingException(error);
		}

		IDxcBlob* pBlob;
		ThrowIfFailed(pResult->GetResult(&pBlob));
		return pBlob;
	}

	//--------------------------------------------------------------------------------------------------
	//
	//
	inline void CreateDescriptorHeap(ID3D12Device* device, const uint32_t count,
	                                                  const D3D12_DESCRIPTOR_HEAP_TYPE type, const bool shaderVisible, ID3D12DescriptorHeap** pHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = count;
		desc.Type = type;
		desc.Flags =
			shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(pHeap)));
	}
} // namespace nv_helpers_dx12