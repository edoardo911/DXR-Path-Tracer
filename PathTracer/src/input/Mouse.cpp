#include "Mouse.h"

namespace RT
{
	Mouse::Mouse()
	{
		buttons = (bool*) calloc(1, sizeof(bool) * SUPPORTED_MOUSE_KEYS);
		lastButtons = (bool*) calloc(1, sizeof(bool) * SUPPORTED_MOUSE_KEYS);
	}

	Mouse::~Mouse()
	{
		free(buttons);
		free(lastButtons);
	}

	void Mouse::onMouseMove(UINT x, UINT y)
	{
		this->x = x;
		this->y = y;
	}

	void Mouse::onButtonDown(MouseButtons button)
	{
		if(button >= 0 && button < SUPPORTED_MOUSE_KEYS)
		{
			lastButtons[button] = buttons[button];
			buttons[button] = true;
		}
	}

	void Mouse::onButtonReleased(MouseButtons button)
	{
		if(button >= 0 && button < SUPPORTED_MOUSE_KEYS)
		{
			lastButtons[button] = buttons[button];
			buttons[button] = false;
		}
	}

	bool Mouse::isButtonDown(MouseButtons button)
	{
		return buttons[button];
	}

	bool Mouse::isButtonClicked(MouseButtons button)
	{
		bool val = lastButtons[button];
		lastButtons[button] = false;
		return val && !buttons[button];
	}
}