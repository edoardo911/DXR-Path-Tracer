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
		UINT32 fps = 60;

		bool fullscreen = false;
		bool vSync = false;

		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		std::wstring title = L"Ray Tracing";

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
	protected:
		settings_struct() = default;

		UINT32 lastW = 1280;
		UINT32 lastH = 720;
	};
}