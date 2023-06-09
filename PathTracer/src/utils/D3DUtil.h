#pragma once

namespace RT
{
    //classes
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

    inline DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber):
        ErrorCode(hr),
        FunctionName(functionName),
        Filename(filename),
        LineNumber(lineNumber)
    {}

    inline std::wstring DxException::ToString() const
    {
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }

    //functions
    inline std::wstring AnsiToWString(const std::string& str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
        return std::wstring(buffer);
    }

    inline static UINT CalcConstantBufferByteSize(UINT byteSize) { return (byteSize + 255) & ~255; }

    inline static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F);

        return I;
    }

    inline static float HaltonSequence(int base, int phase)
    {
        float f = 1.0F;
        float r = 0;
        int i = phase;

        while(i > 0)
        {
            f /= base;
            r += f * (i % base);
            i = (int) ((float) i / base);
        }

        return r;
    }

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