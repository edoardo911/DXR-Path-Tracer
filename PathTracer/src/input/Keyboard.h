#pragma once

#include "../utils/header.h"

//https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
#define KEY_A 0x41
#define KEY_B 0x42
#define KEY_D 0x44
#define KEY_I 0x49
#define KEY_R 0x52
#define KEY_S 0x53
#define KEY_T 0x54
#define KEY_W 0x57
#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_6 0x36
#define KEY_7 0x37
#define KEY_8 0x38
#define KEY_9 0x39

namespace RT
{
	class Keyboard
	{
		friend class Window;
	public:
		~Keyboard();

		bool isKeyDown(UINT key);
		bool isKeyPressed(UINT key);
	protected:
		Keyboard();

		void onKeyPressed(UINT key);
		void onKeyReleased(UINT key);
	private:
		bool* keys;
		bool* lastKeys;
	};
}