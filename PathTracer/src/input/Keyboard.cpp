#include "Keyboard.h"

namespace RT
{
	Keyboard::Keyboard()
	{
		keys = (bool*) calloc(1, sizeof(bool) * SUPPORTED_KEYBOARD_KEYS);
		lastKeys = (bool*) calloc(1, sizeof(bool) * SUPPORTED_KEYBOARD_KEYS);
	}

	Keyboard::~Keyboard()
	{
		free(keys);
		free(lastKeys);
	}

	void Keyboard::onKeyPressed(UINT key)
	{
		if(key >= 0 && key < SUPPORTED_KEYBOARD_KEYS)
		{
			lastKeys[key] = keys[key];
			keys[key] = true;
		}
	}

	void Keyboard::onKeyReleased(UINT key)
	{
		if(key >= 0 && key < SUPPORTED_KEYBOARD_KEYS)
		{
			lastKeys[key] = keys[key];
			keys[key] = false;
		}
	}

	bool Keyboard::isKeyDown(UINT key)
	{
		return keys[key];
	}

	bool Keyboard::isKeyPressed(UINT key)
	{
		bool val = lastKeys[key];
		lastKeys[key] = false;
		return val && !keys[key];
	}
}