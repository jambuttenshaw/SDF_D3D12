#include "pch.h"
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::logger> Log::s_AppLogger;
std::shared_ptr<spdlog::logger> Log::s_D3DLogger;

void Log::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");

	// Create app logger
	{
		s_AppLogger = spdlog::stdout_color_mt("App");
		s_AppLogger->set_level(spdlog::level::info);

		spdlog::sinks::stderr_color_sink_mt* color = static_cast<spdlog::sinks::stderr_color_sink_mt*>(s_AppLogger->sinks().back().get());
		color->set_color(spdlog::level::level_enum::info, 0x0003);	// first 8 bits not relevant for us - background RGBA - RGBA -> 1 blue and 1 alpha

	}

	// Create D3D logger
	{
		s_D3DLogger = spdlog::stdout_color_mt("D3D");
		s_D3DLogger->set_level(spdlog::level::trace);

		spdlog::sinks::stderr_color_sink_mt* color = static_cast<spdlog::sinks::stderr_color_sink_mt*>(s_D3DLogger->sinks().back().get());
		color->set_color(spdlog::level::level_enum::info, 0x0003);	// first 8 bits not relevant for us - background RGBA - RGBA -> 1 blue and 1 alpha
	}

}
