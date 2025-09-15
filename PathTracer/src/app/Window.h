#pragma once

#include "../input/Keyboard.h"
#include "../input/Mouse.h"

#include "../utils/header.h"
#include "../utils/Timer.h"

#include "../rendering/Renderer.h"

namespace RT
{
	class Window
	{
	public:
		inline static Window* getWindow() { return mWindow; }
		inline HINSTANCE windowInst() { return mWindowInst; }
		inline HWND mainWin() { return mMainWin; }

		virtual ~Window();

		bool initialize(std::string sceneName);
		int run();

		virtual LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		inline int getFPS() const { return mFPS; }

		void centerCursor(bool centerMousePos = true);

		inline Renderer* getRenderer() const { return mRenderer.get(); }

		inline void resetFPS()
		{
			if(settings.fps <= 0)
				mFrameTime = 0.0;
			else
				mFrameTime = 1.0 / settings.fps;
		}

		inline void maximize() { mMaximized = true; }

		settings_struct settings {};
	protected:
		static Window* mWindow;

		Window(HINSTANCE inst);
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		virtual void keyboardInput() = 0;
		virtual void mouseInput() = 0;
		virtual void onResize();

		bool initMainWindow();
		
		void calculateFrameStats();

		inline void printPerformance()
		{
			Logger::DEBUG.log("FPS: " + std::to_string(mFPS));
			mRenderer->printMemoryInfo();
		}

		HINSTANCE mWindowInst = nullptr;
		HWND mMainWin = nullptr;

		bool mWindowPaused = false;
		bool mMinimized = false;
		bool mMaximized = false;
		bool mResizing = false;

		Keyboard keyboard{};
		Mouse mouse{};

		Timer mTimer{};

		std::unique_ptr<Renderer> mRenderer;
	private:
		double mFrameTime = 0.0;
		int mFPS = 0;

		void loadEngineSettings();
		void loadSettings();
		void saveSettings();
	};

	Window* createApp(HINSTANCE inst);
}