#include "utils/header.h"

#include "logging/Logger.h"

#include "app/Window.h"

using namespace RT;

class App: public Window
{
public:
	App(HINSTANCE inst): Window(inst) {}

	~App() override = default;

	void keyboardInput() override
	{
		const float dt = mTimer.deltaTime();

		if(keyboard.isKeyDown(VK_CONTROL))
		{
			if(keyboard.isKeyPressed(KEY_I))
				printPerformance();
		}

		if(keyboard.isKeyDown(KEY_W))
			mRenderer->walk(5 * dt);
		if(keyboard.isKeyDown(KEY_A))
			mRenderer->strafe(-5 * dt);
		if(keyboard.isKeyDown(KEY_S))
			mRenderer->walk(-5 * dt);
		if(keyboard.isKeyDown(KEY_D))
			mRenderer->strafe(5 * dt);

		if(keyboard.isKeyPressed(VK_F11))
			mRenderer->toggleFullscreen();
		if(keyboard.isKeyPressed(KEY_1))
			settings.vSync = !settings.vSync;

		if(keyboard.isKeyPressed(VK_ESCAPE))
		{
			mRenderer->flushCommandQueue();
			PostQuitMessage(0);
		}
	}

	void mouseInput() override
	{
		float sens = settings.mouseSensitivity;
		if(settings.vSync && settings.fullscreen) sens *= 2.0F;

		float dx = mTimer.deltaTime() * DirectX::XMConvertToRadians(sens * (static_cast<float>(mouse.getMousePos().x) - (settings.width / 2)));
		float dy = mTimer.deltaTime() * DirectX::XMConvertToRadians(sens * (static_cast<float>(mouse.getMousePos().y) - (settings.height / 2)));

		if(dx != 0 || dy != 0)
			centerCursor(false);
		if(dx != 0)
			mRenderer->getCamera()->rotateY(dx);
		if(dy != 0)
			mRenderer->getCamera()->pitch(dy);
	}

	void onResize() override { Window::onResize(); }
};

void exitDefault()
{
	system("PAUSE");
	FreeConsole();
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstance, _In_ PSTR cmdLine, _In_ int showCmd)
{
	AllocConsole();
	RT::Logger::setup();

	int result;

	try
	{
		App app(hInstance);

		//** Set application settings here **
		//app.settings.dlss = DLSS_QUALITY;
		//app.settings.rayReconstruction = true;

		if(!app.initialize("box")) //scene name
		{
			exitDefault();
			return EXIT_FAILURE;
		}
		result = app.run();
	}
	catch(DLSSException e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		RT::Logger::ERR.log(errorString);
		MessageBox(0, errorString.c_str(), L"DLSS Exception", MB_OK);
		exitDefault();
		return -3;
	}
	catch(RaytracingException e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		RT::Logger::ERR.log(errorString);
		MessageBox(0, errorString.c_str(), L"Raytracing Exception", MB_OK);
		exitDefault();
		return -2;
	}
	catch(DxException e)
	{
		RT::Logger::ERR.log(e.ToString());
		MessageBox(0, e.ToString().c_str(), L"DirectX Exception", MB_OK);
		exitDefault();
		return -1;
	}
	catch(std::exception e)
	{
		std::wstring errorString = AnsiToWString(std::string(e.what()));
		RT::Logger::ERR.log(errorString);
		MessageBox(0, errorString.c_str(), L"Generic Exception", MB_OK);
		exitDefault();
		return 1;
	}

	Logger::INFO.log("Shutting down...");
	exitDefault();
	return result;
}