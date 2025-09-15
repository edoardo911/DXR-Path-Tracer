#pragma once

//win32 libraries
#include <WindowsX.h>
#include <wrl.h>

//directx 12
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <initguid.h>
#include "d3dx12.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d11_1.h>
#include <d3d12.h>
#pragma comment(lib, "D3D12.lib")

#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

//DLSS
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_dlssd.h>

//denoiser
#include <NRD.h>

#include <NRDSettings.h>
#include <NRDDescs.h>

//standard libraries
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <comdef.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <vector>
#include <memory>
#include <string>
#include <locale>
#include <array>
#include <ppl.h>

//macros
#ifndef ThrowIfFailed
	#define ThrowIfFailed(x)													\
	{																			\
		HRESULT hr__ = (x);														\
		std::wstring wfn = RT::AnsiToWString(__FILE__);							\
		if(FAILED(hr__)) { throw RT::DxException(hr__, L#x, wfn, __LINE__); }	\
	}
#endif

//engine libraries
#pragma warning(disable : 4251)

#include "../logging/Logger.h"
#include "exceptions.h"
#include "settings.h"
#include "utils.h"
#include "shader_data.h"
#include "keys.h"