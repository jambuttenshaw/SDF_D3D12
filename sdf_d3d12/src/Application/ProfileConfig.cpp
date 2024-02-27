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
		auto& demoConfig = profileConfig.DemoConfigs[demo];

		// Load demo
		JSON_CHECK_CONTAINS(demoJSON, "name");
		demoConfig.DemoName = demoJSON["name"];

		JSON_CHECK_CONTAINS(demoJSON, "initial_brick_size");
		demoConfig.InitialBrickSize = demoJSON["initial_brick_size"];

		JSON_CHECK_CONTAINS(demoJSON, "linear_increment");
		const bool linearIncrement = demoJSON["linear_increment"];
		demoConfig.IsLinearIncrement = linearIncrement;
		if (linearIncrement)
		{
			JSON_CHECK_CONTAINS(demoJSON, "brick_size_increment");
			demoConfig.BrickSizeIncrement = demoJSON["brick_size_increment"];
		}
		else
		{
			JSON_CHECK_CONTAINS(demoJSON, "brick_size_multiplier");
			demoConfig.BrickSizeMultiplier = demoJSON["brick_size_multiplier"];
		}
		JSON_CHECK_CONTAINS(demoJSON, "num_iterations");
		demoConfig.IterationCount = demoJSON["num_iterations"];

		// Camera properties are optional
		// Init them with defaults in case they are not specified
		demoConfig.CameraConfig.FocalPoint = { 0.0f, 0.0f, 0.0f };
		demoConfig.CameraConfig.OrbitRadius = 10.0f;
		if (demoJSON.contains("camera"))
		{
			const json& cameraJSON = demoJSON["camera"];
			if (cameraJSON.contains("focal_point"))
			{
				if (cameraJSON["focal_point"].size() != 3)
					JSON_ERROR("Focal point is invalid - it should be a 3 element vector.");
				demoConfig.CameraConfig.FocalPoint.x = cameraJSON["focal_point"][0];
				demoConfig.CameraConfig.FocalPoint.y = cameraJSON["focal_point"][1];
				demoConfig.CameraConfig.FocalPoint.z = cameraJSON["focal_point"][2];
			}
			if (cameraJSON.contains("orbit_radius"))
			{
				demoConfig.CameraConfig.OrbitRadius = cameraJSON["orbit_radius"];
			}

		}
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

	gpuProfilerArgs.Headers.reserve(metricsJSON.size());
	gpuProfilerArgs.Metrics.reserve(metricsJSON.size());
	for (const json& metric : metricsJSON)
	{
		if (metric.size() != 2)
			JSON_ERROR("Metrics should be 2 elemenet arrays: [header, metric name].")
		gpuProfilerArgs.Headers.push_back(metric[0]);
		gpuProfilerArgs.Metrics.push_back(metric[1]);
	}

	return true;
}
