#pragma once

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class Log
{
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};


// Convenience logging macros

#ifdef _DEBUG

#define LOG_FATAL(...)	{ ::Log::GetLogger()->critical(__VA_ARGS__); throw std::exception(); }
#define LOG_ERROR(...)	::Log::GetLogger()->error(__VA_ARGS__);
#define LOG_WARN(...)	::Log::GetLogger()->warn(__VA_ARGS__);
#define LOG_INFO(...)	::Log::GetLogger()->info(__VA_ARGS__);

#else

#define LOG_FATAL(...)	
#define LOG_ERROR(...)	
#define LOG_WARN(...)	
#define LOG_INFO(...)	

#endif
