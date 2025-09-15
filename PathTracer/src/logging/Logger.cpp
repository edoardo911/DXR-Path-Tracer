#include "Logger.h"

#include <ctime>

namespace RT
{
	FILE* Logger::stream;

	void Logger::setup()
	{
		INFO.setup(GetStdHandle(STD_OUTPUT_HANDLE));
		WARN.setup(GetStdHandle(STD_OUTPUT_HANDLE));
		ERR.setup(GetStdHandle(STD_OUTPUT_HANDLE));
		DEBUG.setup(GetStdHandle(STD_OUTPUT_HANDLE));

		errno_t err = freopen_s(&stream, "CONOUT$", "w", stdout);
		if(err != 0)
			::OutputDebugString(L"Error reassigning stdout to custom console window");
	}

	void Log::log(std::string string, bool breakLine, bool additionalInfo)
	{
		std::string output;
		std::string end = breakLine ? "\n" : "";

		switch(severity)
		{
		case INFO:
			output = "[INFO]    ";
			break;
		case WARN:
			SetConsoleTextAttribute(consoleHandle, 14);
			output = "[WARNING] ";
			break;
		case ERR:
			SetConsoleTextAttribute(consoleHandle, 12);
			output = "[ERROR]   ";
			break;
		case DEBUG:
			SetConsoleTextAttribute(consoleHandle, 3);
			output = "[DEBUG]   ";
			break;
		}

		if(additionalInfo)
			printf((output + getTime() + string + end).c_str());
		else
			printf((string + end).c_str());

		SetConsoleTextAttribute(consoleHandle, 7);
	}

	void Log::log(std::wstring string, bool breakLine, bool additionalInfo)
	{
		std::string str;
		size_t size;
		str.resize(string.length());
		wcstombs_s(&size, &str[0], str.size() + 1, string.c_str(), string.size());

		Log::log(str, breakLine, additionalInfo);
	}

	void Log::log(int value, bool breakLine, bool additionalInfo) { Log::log(std::to_string(value), breakLine, additionalInfo); }
	void Log::log(float value, bool breakLine, bool additionalInfo) { Log::log(std::to_string(value), breakLine, additionalInfo); }

	std::string Log::getTime()
	{
		std::time_t rawTime;
		std::tm timeInfo;

		std::wstring time;
		char buffer[30];

		std::time(&rawTime);
		localtime_s(&timeInfo, &rawTime);
		std::strftime(buffer, 30, "%d/%m/%Y (%H:%M:%S)", &timeInfo);

		return "[" + std::string(buffer) + "] ";
	}
}