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

		inline POINT getMousePos() const { return { x, y }; }
		inline void setPos(INT32 x, INT32 y) { this->x = x; this->y = y; }
	protected:
		Mouse();

		void onMouseMove(UINT x, UINT y);
		void onButtonDown(MouseButtons button);
		void onButtonReleased(MouseButtons button);

		bool* buttons;
		bool* lastButtons;
		INT32 x = 0;
		INT32 y = 0;
	};
}