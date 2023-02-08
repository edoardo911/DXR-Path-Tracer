#pragma once

#define SUPPORTED_KEYBOARD_KEYS 256
#define SUPPORTED_MOUSE_KEYS	3

#define NUM_FRAME_RESOURCES		3

#define DLSS_OFF				0
#define DLSS_PERFORMANCE		1
#define DLSS_QUALITY			2
#define DLSS_BALANCED			3
#define DLSS_ULTRA_PERFORMANCE	4
#define DLSS_ULTRA_QUALITY		5

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
		UINT32 dlssWidth = 0;
		UINT32 dlssHeight = 0;
		UINT8 fps = 60;
		UINT8 dlss = DLSS_BALANCED;
		UINT8 RTAA = 4;

		bool vSync = false;
		bool fullscreen = false;
		bool dlssSupported = false;

		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		std::wstring name = L"Path Tracing";
	protected:
		settings_struct() = default;

		inline void saveSize()
		{
			lastW = width;
			lastH = height;
		}

		inline void restoreSize()
		{
			width = lastW;
			height = lastH;
		}

		inline void load()
		{
			std::fstream config;
			config.open("res/settings.conf", std::ios::in);

			if(config)
			{
				for(std::string line; std::getline(config, line, '\n'); )
				{
					std::string token = line.substr(0, line.find('='));
					if(token == "width")
						width = std::stoi(line.substr(line.find('=') + 1));
					else if(token == "height")
						height = std::stoi(line.substr(line.find('=') + 1));
					else if(token == "fps")
						fps = std::stoi(line.substr(line.find('=') + 1));
					else if(token == "dlss")
						dlss = std::stoi(line.substr(line.find('=') + 1));
					else if(token == "rtaa")
						RTAA = std::stoi(line.substr(line.find('=') + 1));
					else if(token == "vsync")
						vSync = (line.substr(line.find('=') + 1) == "false") ? false : true;
					else if(token == "fullscreen")
						fullscreen = (line.substr(line.find('=') + 1) == "false") ? false : true;
				}
			}
			else
				throw std::exception("Config file not found");
		}

		UINT32 lastW = 1280;
		UINT32 lastH = 720;
	};
}