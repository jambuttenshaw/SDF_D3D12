#pragma once

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT

#include <spdlog/spdlog.h>

class Log
{
public:
	static void Init();
	inline static std::shared_ptr<spdlog::logger>& GetAppLogger() { return s_AppLogger; }
	inline static std::shared_ptr<spdlog::logger>& GetD3DLogger() { return s_D3DLogger; }

private:
	static std::shared_ptr<spdlog::logger> s_AppLogger;
	static std::shared_ptr<spdlog::logger> s_D3DLogger;
};


// Convenience logging macros

#ifdef _DEBUG

#define LOG_FATAL(...)	{::Log::GetAppLogger()->critical(__VA_ARGS__); throw;}
#define LOG_ERROR(...)	::Log::GetAppLogger()->error(__VA_ARGS__);
#define LOG_WARN(...)	::Log::GetAppLogger()->warn(__VA_ARGS__);
#define LOG_INFO(...)	::Log::GetAppLogger()->info(__VA_ARGS__);
#define LOG_TRACE(...)	::Log::GetAppLogger()->trace(__VA_ARGS__);

#define D3D_LOG_FATAL(...)	{::Log::GetD3DLogger()->critical(__VA_ARGS__); throw;}
#define D3D_LOG_ERROR(...)	::Log::GetD3DLogger()->error(__VA_ARGS__);
#define D3D_LOG_WARN(...)	::Log::GetD3DLogger()->warn(__VA_ARGS__);
#define D3D_LOG_INFO(...)	::Log::GetD3DLogger()->info(__VA_ARGS__);
#define D3D_LOG_TRACE(...)	::Log::GetD3DLogger()->trace(__VA_ARGS__);

#else

#define LOG_FATAL(...)	
#define LOG_ERROR(...)	
#define LOG_WARN(...)	
#define LOG_INFO(...)	
#define LOG_TRACE(...)

#define D3D_LOG_FATAL(...)
#define D3D_LOG_ERROR(...)
#define D3D_LOG_WARN(...)
#define D3D_LOG_INFO(...)
#define D3D_LOG_TRACE(...)
#endif

