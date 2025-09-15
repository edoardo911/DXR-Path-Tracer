#pragma once

namespace RT
{
	class DxException
	{
	public:
		inline DxException() = default;
		inline DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& fileName, int lineNumber):
			ErrorCode(hr), FunctionName(functionName), Filename(fileName), LineNumber(lineNumber) {}

		inline virtual std::wstring ToString() const
		{
			_com_error err(ErrorCode);
			std::wstring msg = err.ErrorMessage();

			return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
		}

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};

	class Win32Exception: public std::exception
	{
	public:
		explicit inline Win32Exception(const char* message): msg(message) {}
		explicit inline Win32Exception(const std::string& message): msg(message) {}
		inline ~Win32Exception() noexcept {}
		const char* what() const noexcept override { return msg.c_str(); }
	private:
		std::string msg;
	};

	class DLSSException: public std::exception
	{
	public:
		explicit inline DLSSException(const char* message): msg(message) {}
		explicit inline DLSSException(const std::string& message): msg(message) {}
		inline ~DLSSException() noexcept {}
		const char* what() const noexcept override { return msg.c_str(); }
	private:
		std::string msg;
	};

	class RaytracingException: public std::exception
	{
	public:
		explicit inline RaytracingException(const char* message): msg(message) {}
		explicit inline RaytracingException(const std::string& message) : msg(message) {}
		inline ~RaytracingException() noexcept {}
		const char* what() const noexcept override { return msg.c_str(); }
	private:
		std::string msg;
	};
}