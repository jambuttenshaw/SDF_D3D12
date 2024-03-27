#pragma once

#include <args.hxx>

#include "ProfileConfig.h"
#include "Renderer/Profiling/GPUProfiler.h"


class D3DApplication;


class ProfilingDataCollector
{
public:
	ProfilingDataCollector(D3DApplication* application);
	~ProfilingDataCollector() = default;

	DISALLOW_COPY(ProfilingDataCollector)
	DEFAULT_MOVE(ProfilingDataCollector)

	void ParseCommandLineArgs(args::Subparser& subparser);
	// Returns whether profiling mode is enabled
	bool InitProfiler();

	void UpdateProfiling(class Scene* scene, 
						 class GameTimer* timer, 
						 class CameraController* cameraController);

	// Getters
	inline bool IsInProfilingMode() const { return m_ProfilingMode; }
	inline const DemoConfig& GetDemoConfig(UINT index) const { return m_ProfileConfig.DemoConfigs[index]; }

private:
	D3DApplication* m_Application = nullptr;

	// Profiling configuration
	bool m_ProfilingMode = false;
	ProfileConfig m_ProfileConfig;

	UINT m_CapturesRemaining = 0;
	bool m_BegunCapture = false;

	UINT m_DemoIterationsRemaining = 0;
	UINT m_DemoConfigIndex = 0;

	float m_DemoBeginTime = 0.0f;
	float m_DemoWarmupTime = 5.0f;

	std::string m_ConfigData;

	bool m_LoadDefaultGPUProfilerArgs = true;		// If no config was specified in the command line args, default config will be loaded
	GPUProfilerArgs m_GPUProfilerArgs;

	bool m_OutputAvailableMetrics = false;
	std::string m_AvailableMetricsOutfile;
};
