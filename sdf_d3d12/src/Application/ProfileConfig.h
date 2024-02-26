#pragma once
#include "Renderer/Profiling/GPUProfiler.h"


// Application-specific arguments
struct DemoConfig
{
	// Which demo scene to run
	std::string DemoName;

	// How to manipulate variables in this demo scene
	float InitialBrickSize;
	bool IsLinearIncrement;
	union
	{
		float BrickSizeIncrement;
		float BrickSizeMultiplier;
	};
	UINT IterationCount;
};


struct ProfileConfig
{
	// All of the different demos to run
	std::vector<DemoConfig> DemoConfigs;

	// Run info
	// The number of times to run each configuration to gather an average
	UINT NumCaptures;

	// Output info
	std::string OutputFile;
};


// JSON Loaders
bool ParseProfileConfigFromJSON(const std::string& path, ProfileConfig& profileConfig);
bool ParseGPUProfilerArgsFromJSON(const std::string& path, GPUProfilerArgs& gpuProfilerArgs);
