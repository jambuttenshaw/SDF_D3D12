#include "pch.h"
#include "ProfileConfig.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// For convenient error printing and returning
#define JSON_ERROR(...) { LOG_ERROR(__VA_ARGS__); return false; }
#define JSON_CHECK_CONTAINS(json, name) if (!(json).contains(name)) JSON_ERROR("Missing field: {}", name)


bool ParseJSON(json& doc, std::ifstream& file)
{
	try
	{
		doc = json::parse(file);
	}
	catch (const json::parse_error& e)
	{
		LOG_ERROR(e.what());
		return false;
	}
	catch (const json::type_error& e)
	{
		LOG_ERROR(e.what());
		return false;
	}
	catch (const json::other_error& e)
	{
		LOG_ERROR(e.what());
		return false;
	}

	return true;
}


bool ParseProfileConfigFromJSON(const std::string& path, ProfileConfig& profileConfig)
{
	// Attempt to read supplied config path
	std::ifstream file(path.c_str());
	if (!file.good())
	{
		LOG_ERROR("Failed to load path: {}", path.c_str());
		return false;
	}

	json docJSON;
	if (!ParseJSON(docJSON, file))
		return false;

	// Load all demos
	if (!docJSON.contains("demos"))
		JSON_ERROR("Config must contain at least 1 demo!");

	const json& demos = docJSON["demos"];
	profileConfig.DemoConfigs.resize(demos.size());

	for (size_t demo = 0; demo < demos.size(); demo++)
	{
		const json& demoJSON = demos[demo];

		// Load demo
		JSON_CHECK_CONTAINS(demoJSON, "name");
		profileConfig.DemoConfigs[demo].DemoName = demoJSON["name"];

		JSON_CHECK_CONTAINS(demoJSON, "initial_brick_size");
		profileConfig.DemoConfigs[demo].InitialBrickSize = demoJSON["initial_brick_size"];

		JSON_CHECK_CONTAINS(demoJSON, "linear_increment");
		const bool linearIncrement = demoJSON["linear_increment"];
		profileConfig.DemoConfigs[demo].IsLinearIncrement = linearIncrement;
		if (linearIncrement)
		{
			JSON_CHECK_CONTAINS(demoJSON, "brick_size_increment");
			profileConfig.DemoConfigs[demo].BrickSizeIncrement = demoJSON["brick_size_increment"];
		}
		else
		{
			JSON_CHECK_CONTAINS(demoJSON, "brick_size_multiplier");
			profileConfig.DemoConfigs[demo].BrickSizeMultiplier = demoJSON["brick_size_multiplier"];
		}

		JSON_CHECK_CONTAINS(demoJSON, "iteration_count");
		profileConfig.DemoConfigs[demo].IterationCount = demoJSON["iteration_count"];
	}

	// Load run parameters
	JSON_CHECK_CONTAINS(docJSON, "num_captures");
	profileConfig.NumCaptures = docJSON["num_captures"];

	// Load output info
	JSON_CHECK_CONTAINS(docJSON, "output_file");
	profileConfig.OutputFile = docJSON["output_file"];

	return true;
}


bool ParseGPUProfilerArgsFromJSON(const std::string& path, GPUProfilerArgs& gpuProfilerArgs)
{
	// Attempt to read supplied config path
	std::ifstream file(path.c_str());
	if (!file.good())
	{
		LOG_ERROR("Failed to load path: {}", path.c_str());
		return false;
	}

	json docJSON;
	if (!ParseJSON(docJSON, file))
		return false;

	JSON_CHECK_CONTAINS(docJSON, "queue");
	const std::string& queue = docJSON["queue"];
	if (queue == "direct")
		gpuProfilerArgs.Queue = GPUProfilerQueue::Direct;
	else if (queue == "compute")
		gpuProfilerArgs.Queue = GPUProfilerQueue::Compute;
	else
	{
		LOG_ERROR("Invalid queue name: '{}'. Queue must be 'direct' or 'compute'.", queue);
		return false;
	}

	JSON_CHECK_CONTAINS(docJSON, "metrics");
	const json& metricsJSON = docJSON["metrics"];

	gpuProfilerArgs.Metrics.reserve(metricsJSON.size());
	for (std::string metric : metricsJSON)
	{
		gpuProfilerArgs.Metrics.emplace_back(std::move(metric));
	}

	if (docJSON.contains("headers"))
	{
		const json& headersJSON = docJSON["headers"];
		if (headersJSON.size() != metricsJSON.size())
			JSON_ERROR("Headers and metrics do not match.");

		gpuProfilerArgs.Headers.reserve(headersJSON.size());
		for (std::string header : headersJSON)
		{
			gpuProfilerArgs.Headers.emplace_back(std::move(header));
		}
	}
	else
	{
		gpuProfilerArgs.Headers = gpuProfilerArgs.Metrics;
	}

	return true;
}
