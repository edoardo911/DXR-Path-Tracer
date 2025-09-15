#pragma once

#include "../utils/header.h"

namespace RT
{
	class Mouse
	{
		friend class Window;
	public:
		~Mouse();

		bool isButtonDown(MouseButtons button);
		bool isButtonClicked(MouseButtons button);
		inline POINT getMousePos() const { return { x,y }; }
		inline void setPos(INT32 x, INT32 y) { this->x = x; this->y = y; }
		inline void clearWheel() { wheel = 0; }
		inline INT32 getWheel() const { return wheel; }
	protected:
		Mouse();

		void clearStates();

		void onMouseMove(UINT x, UINT y);
		void onButtonDown(MouseButtons button);
		void onButtonRelease(MouseButtons button);
		void onWheel(float amount);
	private:
		bool* buttons;
		bool* lastButtons;

		INT32 x = 0;
		INT32 y = 0;
		INT32 wheel = 0;
	};
}