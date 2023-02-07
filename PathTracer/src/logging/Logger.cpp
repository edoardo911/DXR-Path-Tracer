#include "Logger.h"

#include <ctime>

namespace RT
{
	void Log::log(std::string string)
	{
	#ifdef _DEBUG
		std::wstring wide;
		std::wstring prefix = L"";
		std::wstring time = getTime();

		if(severity == INFO)
			prefix = L"[INFO]";
		else if(severity == WARN)
			prefix = L"[WARNING]";
		else if(severity == ERR)
			prefix = L"[ERROR]";

		int convertResult = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), (int) string.length(), NULL, 0);
		wide.resize(convertResult);
		MultiByteToWideChar(CP_UTF8, 0, string.c_str(), (int) string.length(), &wide[0], (int) wide.size());

		::OutputDebugString((prefix + time + wide + L"\n").c_str());
	#endif
	}

	void Log::log(std::wstring string)
	{
	#ifdef _DEBUG
		std::wstring prefix;
		std::wstring time = getTime();

		if(severity == INFO)
			prefix = L"[INFO]";
		else if(severity == WARN)
			prefix = L"[WARNING]";
		else if(severity == ERR)
			prefix = L"[ERROR]";

		::OutputDebugString((prefix + time + string + L"\n").c_str());
	#endif
	}

	std::wstring Log::getTime()
	{
		std::time_t rawTime;
		std::tm timeInfo;

		std::wstring time;
		char buffer[30];

		std::time(&rawTime);
		localtime_s(&timeInfo, &rawTime);
		std::strftime(buffer, 30, "%d/%m/%Y (%H:%M:%S)", &timeInfo);

		int convertResult = MultiByteToWideChar(CP_UTF8, 0, buffer, (int) strlen(buffer), NULL, 0);
		time.resize(convertResult);
		MultiByteToWideChar(CP_UTF8, 0, buffer, (int) strlen(buffer), &time[0], (int) time.size());

		return L"[" + time + L"] ";
	}

	void Log::log(int value) { Log::log(std::to_wstring(value)); }
	void Log::log(float value) { Log::log(std::to_wstring(value)); }
}