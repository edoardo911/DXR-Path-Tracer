#pragma once

#define SUPPORTED_KEYBOARD_KEYS 256
#define SUPPORTED_MOUSE_KEYS 3

#define NUM_FRAME_RESOURCES 3

enum MouseButtons
{
	MOUSE_LBUTTON = 0,
	MOUSE_MBUTTON,
	MOUSE_RBUTTON
};

namespace RT
{
	struct settings_struct
	{
		friend class Window;
	public:
		UINT32 width = 1280;
		UINT32 height = 720;
		UINT8 fps = 60;

		bool vSync = false;
		bool fullscreen = false;

		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		std::wstring name = L"Path Tracing";
	protected:
		settings_struct() = default;

		void saveSize()
		{
			lastW = width;
			lastH = height;
		}

		void restoreSize()
		{
			width = lastW;
			height = lastH;
		}

		UINT32 lastW = 1280;
		UINT32 lastH = 720;
	};
}