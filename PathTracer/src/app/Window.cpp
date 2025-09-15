#include "Window.h"

#include "../rendering/RaytracingRenderer.h"

namespace RT
{
	Window* Window::mWindow = nullptr;

	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if(Window::getWindow())
			return Window::getWindow()->msgProc(hwnd, msg, wParam, lParam);
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	LRESULT Window::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch(msg)
		{
		case WM_ACTIVATE:
			if(LOWORD(wParam) == WA_INACTIVE)
			{
				mWindowPaused = true;
				mTimer.stop();

				if(settings.fullscreen)
				{
					mRenderer->toggleFullscreen();
					ShowWindow(mMainWin, SW_SHOWMINIMIZED);
				}
			}
			else
			{
				mWindowPaused = false;
				mTimer.start();
			}
			return 0;
		case WM_SIZE:
			settings.width = LOWORD(lParam);
			settings.height = HIWORD(lParam);

			if(mRenderer->getDevice())
			{
				if(wParam == SIZE_MINIMIZED)
				{
					mWindowPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if(wParam == SIZE_MAXIMIZED)
				{
					mWindowPaused = false;
					mMinimized = false;
					mMaximized = true;
					onResize();
				}
				else if(wParam == SIZE_RESTORED)
				{
					if(mMinimized)
					{
						mWindowPaused = false;
						mMinimized = false;
						onResize();
					}
					else if(mMaximized)
					{
						mWindowPaused = false;
						mMaximized = false;
						onResize();
					}
					else if(!mResizing)
						onResize();
				}
			}
			return 0;
		case WM_ENTERSIZEMOVE:
			mWindowPaused = true;
			mResizing = true;
			mTimer.stop();
			return 0;
		case WM_EXITSIZEMOVE:
			mWindowPaused = false;
			mResizing = false;
			mTimer.start();
			onResize();
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_MENUCHAR:
			return MAKELRESULT(0, MNC_CLOSE);
		case WM_GETMINMAXINFO:
			((MINMAXINFO*) lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*) lParam)->ptMinTrackSize.y = 200;
			return 0;
		case WM_LBUTTONDOWN:
			mouse.onButtonDown(MOUSE_LBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_MBUTTONDOWN:
			mouse.onButtonDown(MOUSE_MBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_RBUTTONDOWN:
			mouse.onButtonDown(MOUSE_RBUTTON);
			SetCapture(mMainWin);
			return 0;
		case WM_LBUTTONUP:
			mouse.onButtonRelease(MOUSE_LBUTTON);
			ReleaseCapture();
			return 0;
		case WM_MBUTTONUP:
			mouse.onButtonRelease(MOUSE_MBUTTON);
			ReleaseCapture();
			return 0;
		case WM_RBUTTONUP:
			mouse.onButtonRelease(MOUSE_RBUTTON);
			ReleaseCapture();
			return 0;
		case WM_MOUSEMOVE:
			mouse.onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		case WM_MOUSEWHEEL:
			mouse.onWheel(GET_WHEEL_DELTA_WPARAM(wParam));
			return 0;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			keyboard.onKeyPressed(static_cast<UINT>(wParam));
			return 0;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			keyboard.onKeyReleased(static_cast<UINT>(wParam));
			return 0;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	Window::Window(HINSTANCE inst): mWindowInst(inst)
	{
		assert(mWindow == nullptr);
		mWindow = this;

		loadSettings();
	}

	Window::~Window()
	{
		mRenderer.reset();
		DestroyWindow(mMainWin);
		UnregisterClassW(L"UnoGameEngineWindow", mWindowInst);
		mWindow = nullptr;
	}

	int Window::run()
	{
		MSG msg = { 0 };
		double counter = 0.0;
		resetFPS();
		mTimer.reset();

		while(msg.message != WM_QUIT)
		{
			if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				mTimer.tick();
				if(!mWindowPaused)
				{
					keyboardInput();
					mouseInput();

					mRenderer->update(mTimer.deltaTime());

					mouse.clearWheel();
					mouse.clearStates();
					keyboard.clearStates();

					counter += mTimer.deltaTime();
					if(counter > mFrameTime)
					{
						counter -= mFrameTime;
						if(!mRenderer->frameInExecution())
						{
						#ifndef UGE_DIST
							calculateFrameStats();
						#endif

							mRenderer->updateFrameData();
							mRenderer->draw();
						}
					}
				}
				else
					Sleep(500);
			}
		}

		saveSettings();
		return 0;
	}

	bool Window::initialize(std::string sceneName)
	{
		loadEngineSettings();

		if(settings.dlss)
		{
			settings.fxaa = false;
		}
		else
			settings.rayReconstruction = false;

		Logger::INFO.log("Window initialization...");
		mRenderer = std::make_unique<RaytracingRenderer>(&settings);

		if(!initMainWindow())
			return false;
		if(!mRenderer->initDX12(mMainWin))
			return false;

		settings.dlssSupported = mRenderer->initDLSS();
		if(!settings.dlssSupported)
		{
			Logger::WARN.log("DLSS not supported");
			settings.dlss = DLSS_OFF;
			settings.rayReconstruction = false;
		}

		if(!mRenderer->initContext(sceneName))
			return false;

		centerCursor();
		ShowCursor(FALSE);

		//get system info
		GetPhysicallyInstalledSystemMemory(&settings.ramBytes);

		int CPUinfo[4] = { -1 };
		unsigned nExIds, i = 0;
		char CPUBrandString[64] = "";

		__cpuid(CPUinfo, 0x80000000);
		nExIds = CPUinfo[0];
		for(i = 0x80000000; i < nExIds; ++i)
		{
			__cpuid(CPUinfo, i);
			if(i == 0x80000002)
				memcpy(CPUBrandString, CPUinfo, sizeof(CPUinfo));
			else if(i == 0x80000003)
				memcpy(CPUBrandString + 16, CPUinfo, sizeof(CPUinfo));
			else if(i == 0x80000004)
				memcpy(CPUBrandString + 32, CPUinfo, sizeof(CPUinfo));
		}

		int size = MultiByteToWideChar(CP_UTF8, 0, CPUBrandString, (int) strlen(CPUBrandString), NULL, 0);
		settings.cpuInfo.resize(size);
		MultiByteToWideChar(CP_UTF8, 0, CPUBrandString, (int) strlen(CPUBrandString), &settings.cpuInfo[0], size);

		Logger::INFO.log(L"Detected system with \"" + settings.cpuInfo + L"\" and a memory with " + std::to_wstring(settings.ramBytes / pow(2, 20)) + L"GB");

		//get physical cores
		UINT logicalCores = std::thread::hardware_concurrency();
		if(logicalCores == 0)
			throw Win32Exception("Couldn't determine the number of logical cores of the CPU");
		Logger::INFO.log(L"Logical cores: " + std::to_wstring(logicalCores) + L", max threads: " + std::to_wstring(logicalCores - 1));

		onResize();

		if(settings.fullscreen)
		{
			settings.fullscreen = false;
			mRenderer->toggleFullscreen();
		}
		if(mMaximized)
		{
			PostMessage(mMainWin, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
			mMaximized = false;
		}
		return true;
	}

	bool Window::initMainWindow()
	{
		Logger::INFO.log("Initializing window...");

		WNDCLASS wc;
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = MainWndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = mWindowInst;
		wc.hIcon = LoadIcon(0, IDI_APPLICATION);
		wc.hCursor = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) GetStockObject(NULL_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = L"UnoGameEngineWindow";

		if(!RegisterClass(&wc))
		{
			Logger::ERR.log("RegisterClass failed");
			MessageBox(0, L"RegisterClass failed", 0, 0);
			return false;
		}

		RECT R = { 0, 0, static_cast<LONG>(settings.width), static_cast<LONG>(settings.height) };
		AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
		int w = R.right - R.left;
		int h = R.bottom - R.top;

		mMainWin = CreateWindow(L"UnoGameEngineWindow", (settings.title + L" - " + settings.version).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, 0, 0, mWindowInst, 0);
		if(!mMainWin)
		{
			Logger::ERR.log("CreateWindow failed");
			MessageBox(0, L"CreateWindow failed", 0, 0);
			return false;
		}

		ShowWindow(mMainWin, SW_SHOW);
		UpdateWindow(mMainWin);
		return true;
	}

	void Window::centerCursor(bool centerMousePos)
	{
		RECT r, f;
		GetWindowRect(mMainWin, &r);
		GetClientRect(mMainWin, &f);
		UINT border = r.bottom - r.top - f.bottom - 8;
		SetCursorPos(r.left + (r.right - r.left) / 2, settings.fullscreen ? (settings.height / 2) : (r.top + border + f.bottom / 2));
		if(centerMousePos)
			mouse.setPos(settings.width / 2, settings.height / 2);
	}

	void Window::calculateFrameStats()
	{
		static int frameCnt = 0;
		static float timeElapsed = 0.0F;

		frameCnt++;
		if((mTimer.totalTime() - timeElapsed) >= 1.0F)
		{
			mFPS = frameCnt;
			frameCnt = 0;
			timeElapsed += 1.0F;

		#ifndef UGE_DIST
			mRenderer->queryVideoInfo();
		#endif
		}
	}

	void Window::onResize() { mRenderer->onResize(); }

	void Window::loadEngineSettings()
	{
		std::ifstream file("res/engine_settings.conf");
		std::string line;
		while(std::getline(file, line))
		{
			std::string param = line.substr(0, line.find("="));
			std::string value = line.substr(line.find("=") + 1, line.length());
		}
	}

	void Window::loadSettings() {}
	void Window::saveSettings() {}
}