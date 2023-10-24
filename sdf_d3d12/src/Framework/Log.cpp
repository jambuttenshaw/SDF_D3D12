#include "Log.h"


std::shared_ptr<spdlog::logger> Log::s_Logger;

void Log::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	s_Logger = spdlog::stdout_color_mt("Log");
	s_Logger->set_level(spdlog::level::trace);

	spdlog::sinks::stderr_color_sink_mt* color = static_cast<spdlog::sinks::stderr_color_sink_mt*>(s_Logger->sinks().back().get());

	color->set_color(spdlog::level::level_enum::info, 0x0003);	// first 8 bits not relevant for us - background RGBA - RGBA -> 1 blue and 1 alpha
	s_Logger = spdlog::stdout_color_mt("App");
	s_Logger->set_level(spdlog::level::trace);
}
