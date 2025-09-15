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

	void Mouse::onButtonRelease(MouseButtons button)
	{
		if(button >= 0 && button < SUPPORTED_MOUSE_KEYS)
		{
			lastButtons[button] = buttons[button];
			buttons[button] = false;
		}
	}

	void Mouse::onWheel(float amount)
	{
		wheel += (INT32) (amount / 120.0F);
	}

	bool Mouse::isButtonDown(MouseButtons button)
	{
		return buttons[button];
	}

	bool Mouse::isButtonClicked(MouseButtons button)
	{
		return lastButtons[button] && !buttons[button];
	}

	void Mouse::clearStates()
	{
		ZeroMemory(lastButtons, sizeof(bool) * SUPPORTED_MOUSE_KEYS);
	}
}