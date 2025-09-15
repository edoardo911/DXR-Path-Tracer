#pragma once

#define MAX_LIGHTS					128
#define MAX_BONE_MATRICES			96

#define SUPPORTED_KEYBOARD_KEYS		256
#define SUPPORTED_MOUSE_KEYS		3
#define SUPPORTED_JOYSTICK_BUTTONS	14
#define SUPPORTED_JOYSTICK_AXES		6
#define JOYSTICK_DEADZONE			6000

namespace RT
{
	enum TexResolution
	{
		TEX_RESOLUTION_2048_X_2048 = 0,
		TEX_RESOLUTION_1024_X_1024,
		TEX_RESOLUTION_512_X_512,
		TEX_RESOLUTION_256_X_256,
		TEX_RESOLUTION_128_X_128,
	};

	enum TexFilter
	{
		TEX_FILTER_NEAREST = 0,
		TEX_FILTER_BILINEAR,
		TEX_FILTER_TRILINEAR
	};

	enum LightType
	{
		LIGHT_TYPE_DIRECTIONAL = 0,
		LIGHT_TYPE_SPOTLIGHT,
		LIGHT_TYPE_POINTLIGHT
	};

	enum ShadowOptions
	{
		SHADOWS_OFF = 0,
		SHADOWS_LOW,
		SHADOWS_MEDIUM,
		SHADOWS_HIGH,
		SHADOWS_MONSTER
	};

	enum Tonemapping
	{
		TONEMAPPING_OFF = 0,
		TONEMAPPING_REINHARD,
		TONEMAPPING_UNCHARTED,
		TONEMAPPING_ACES
	};

	enum DLSSMode
	{
		DLSS_OFF = 0,
		DLSS_PERFORMANCE,
		DLSS_QUALITY,
		DLSS_BALANCED,
		DLSS_ULTRA_PERFORMANCE,
		DLSS_ULTRA_QUALITY,
		DLSS_DLAA
	};

	enum MouseButtons
	{
		MOUSE_LBUTTON = 0,
		MOUSE_MBUTTON,
		MOUSE_RBUTTON
	};

	enum JoystickAxes
	{
		JOYSTICK_LEFTX = 0,
		JOYSTICK_LEFTY,
		JOYSTICK_RIGHTX,
		JOYSTICK_RIGHTY,
		JOYSTICK_L2,
		JOYSTICK_R2
	};

	enum JoystickButtons
	{
		JOYSTICK_UP = 0,
		JOYSTICK_RIGHT,
		JOYSTICK_DOWN,
		JOYSTICK_LEFT,
		JOYSTICK_PAD_UP,
		JOYSTICK_PAD_RIGHT,
		JOYSTICK_PAD_DOWN,
		JOYSTICK_PAD_LEFT,
		JOYSTICK_L1,
		JOYSTICK_L3,
		JOYSTICK_R1,
		JOYSTICK_R3,
		JOYSTICK_SELECT,
		JOYSTICK_START
	};

	struct settings_struct
	{
		friend class Window;
		friend class Renderer;
	public:
		//video settings
		UINT32 width = 1280;
		UINT32 height = 720;
		UINT32 dlssWidth = 0;
		UINT32 dlssHeight = 0;
		UINT16 fps = 60;
		TexFilter texFilter = TEX_FILTER_TRILINEAR;
		TexResolution texResolution = TEX_RESOLUTION_2048_X_2048;
		UINT8 anisotropic = 16;
		DLSSMode dlss = DLSS_OFF;

		//sensitivity
		float mouseSensitivity = 5.0F;

		//flags
		bool vSync = false;
		bool fullscreen = false;
		bool mipmaps = true;
		bool specular = true;
		bool rtao = true;
		bool rtReflections = true;
		bool rtRefractions = true;
		bool rtShadows = true;
		bool indirect = true;

		bool texturing = true;
		bool normalMapping = true;
		bool roughnessMapping = true;
		bool heightMapping = true;
		bool aoMapping = true;
		bool metallicMapping = true;
		bool rayReconstruction = false;

		//effects
		bool fxaa = true;
		bool colorGrading = true;
		bool vignette = true;

		//color adjust
		DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		float exposure = 1.0F;
		float brightness = 0.0F;
		float contrast = 1.0F;
		float saturation = 1.0F;
		float gamma = 1.0F;
		Tonemapping tonemapping = TONEMAPPING_ACES;

		//limitations
		UINT8 maxAnisotropy = 16;
		bool dlssSupported = false;
		bool rayReconstructionSupported = false;

		//info
		std::wstring title = L"DXR Path Tracer";
		std::wstring version = L"dev2.0.0";
		UINT64 ramBytes = 0;
		std::wstring cpuInfo = L"";
		std::wstring gpuInfo = L"";
		float vramGB = 0;

		inline UINT32 getWidth() { return (dlss && dlssSupported) ? dlssWidth : width; }
		inline UINT32 getHeight() { return (dlss && dlssSupported) ? dlssHeight : height; }
	protected:
		settings_struct() = default;

		UINT32 windowedWidth = 1280;
		UINT32 windowedHeight = 720;

		inline void save()
		{
			windowedWidth = width;
			windowedHeight = height;
		}

		inline void load()
		{
			width = windowedWidth;
			height = windowedHeight;
		}
	};
}