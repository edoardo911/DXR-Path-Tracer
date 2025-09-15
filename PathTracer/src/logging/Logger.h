#pragma once

#include <Windows.h>
#include <string>

#include "../utils/header.h"

namespace RT
{
	enum LoggingSeverity
	{
		INFO = 0,
		WARN = 1,
		ERR = 2,
		DEBUG = 3
	};

	class Log
	{
		friend class Logger;
	public:
		Log() = delete;

		void log(std::string string, bool breakLine = true, bool additionalInfo = true);
		void log(std::wstring string, bool breakLine = true, bool additionalInfo = true);
		void log(int value, bool breakLine = true, bool additionalInfo = true);
		void log(float value, bool breakLine = true, bool additionalInfo = true);
	protected:
		LoggingSeverity severity;
		HANDLE consoleHandle = 0;

		inline Log(LoggingSeverity sev): severity(sev) {}

		inline void setup(HANDLE handle) { consoleHandle = handle; }

		std::string getTime();
	};

	class Logger
	{
	public:
		inline static Log INFO{ LoggingSeverity::INFO };
		inline static Log WARN{ LoggingSeverity::WARN };
		inline static Log ERR{ LoggingSeverity::ERR };
		inline static Log DEBUG{ LoggingSeverity::DEBUG };

		static void setup();
	private:
		static FILE* stream;

		Logger() = default;
	};
}