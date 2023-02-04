#pragma once

#include "../utils/header.h"

namespace RT
{
	enum LOGGING_SEVERITY
	{
		INFO = 0,
		WARN = 1,
		ERR = 2
	};

	class Log
	{
		friend class Logger;
	public:
		Log() = delete;

		void log(std::string string);
		void log(std::wstring string);
	protected:
		inline Log(LOGGING_SEVERITY sev): severity(sev) {}

		std::wstring getTime();

		LOGGING_SEVERITY severity;
	};

	class Logger
	{
	public:
		inline static Log INFO{ LOGGING_SEVERITY::INFO };
		inline static Log WARN { LOGGING_SEVERITY::WARN };
		inline static Log ERR{ LOGGING_SEVERITY::ERR };
	private:
		Logger() = default;
	};
}