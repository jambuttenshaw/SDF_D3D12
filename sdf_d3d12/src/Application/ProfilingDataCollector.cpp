#include "pch.h"
#include "ProfilingDataCollector.h"

#include <fstream>
#include <iomanip>

#include "Windows/Win32Application.h"

#include "Scene.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera/OrbitalCameraController.h"


void ProfilingDataCollector::ParseCommandLineArgs(args::Subparser& subparser)
{
#ifdef ENABLE_INSTRUMENTATION
	args::ValueFlag<std::string> profileConfig(subparser, "Profile Config", "Path to profiling config file", { "profile-config" });
	args::ValueFlag<std::string> gpuProfileConfig(subparser, "GPU Profiler Config", "Path to GPU profiler config file", { "gpu-profiler-config" });
	args::ValueFlag<std::string> profileOutput(subparser, "Output", "Path to output file", { "profile-output" });

	args::ValueFlag<std::string> outputAvailableMetrics(subparser, "output-available-metrics", "Output file for available GPU metrics", { "output-available-metrics" });

	subparser.Parse();

	// These settings aren't relevant otherwise
	if (profileConfig)
	{
		// Parse config
		if (ParseProfileConfigFromJSON(profileConfig.Get(), m_ProfileConfig))
		{
			m_ProfilingMode = true;

			const auto& demoConfig = m_ProfileConfig.DemoConfigs[0];

			m_CapturesRemaining = m_ProfileConfig.NumCaptures;
			m_DemoIterationsRemaining = demoConfig.IterationCount;
			m_DemoConfigIndex = 0;

			{
				// Build iteration data string
				std::stringstream stream;
				stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << demoConfig.InitialBrickSize << ",";
				m_ConfigData = std::move(stream.str());
			}
		}
	}

	if (gpuProfileConfig)
	{
		// Parse gpu profiler args
		if (ParseGPUProfilerArgsFromJSON(gpuProfileConfig.Get(), m_GPUProfilerArgs))
		{
			// Don't load defaults - args have been supplied
			m_LoadDefaultGPUProfilerArgs = false;
		}
	}

	if (m_ProfilingMode)
	{
		m_ProfileConfig.OutputFile = profileOutput ? profileOutput.Get() : "captures/profile.csv";
		LOG_INFO("Using profiling output path: '{}'", m_ProfileConfig.OutputFile);
	}

	if (outputAvailableMetrics)
	{
		m_OutputAvailableMetrics = true;
		m_AvailableMetricsOutfile = outputAvailableMetrics.Get();
	}
#endif
}


bool ProfilingDataCollector::InitProfiler()
{
#ifdef ENABLE_INSTRUMENTATION
	if (m_LoadDefaultGPUProfilerArgs)
	{
		m_GPUProfilerArgs.Queue = GPUProfilerQueue::Direct;
		m_GPUProfilerArgs.Metrics.push_back("gpu__time_duration.sum");
		m_GPUProfilerArgs.Headers.push_back("Duration");
	}

	GPUProfiler::Create(m_GPUProfilerArgs);

	if (m_OutputAvailableMetrics)
	{
		GPUProfiler::GetAvailableMetrics(m_AvailableMetricsOutfile);
	}

	if (m_ProfilingMode)
	{
		// Write headers into outfile
		std::ofstream outFile(m_ProfileConfig.OutputFile);
		if (outFile.good())
		{
			outFile << "Demo Name,Brick Size,BrickCount,EditCount,Range Name";
			for (const auto& header : m_GPUProfilerArgs.Headers)
			{
				outFile << "," << header;
			}
			outFile << std::endl;
		}
		else
		{
			LOG_ERROR("Failed to open outfile '{}'", m_ProfileConfig.OutputFile.c_str());
		}
	}

	return m_ProfilingMode;
#else
	return false;
#endif
}



void ProfilingDataCollector::UpdateProfiling(Scene* scene, GameTimer* timer, CameraController* cameraController)
{
#ifdef ENABLE_INSTRUMENTATION
	// This function keeps track of gathering profiling data as specified by the config
	// If no config was given, then no profiling will take place and the application will run in demo mode as normal
	if (!m_ProfilingMode)
		return;

	if (timer->GetTimeSinceReset() < GPUProfiler::GetWarmupTime())
		return;

	if (timer->GetTimeSinceReset() - m_DemoBeginTime < m_DemoWarmupTime)
		return;
	scene->SetPaused(true);

	// Still performing runs to gather data
	if (m_CapturesRemaining > 0)
	{
		// It is safe to access profiler within this function - this will only be called if instrumentation is enabled
		if (GPUProfiler::Get().IsInCollection())
		{
			// If it is in collection, check if it has finished collecting data
			std::vector<std::stringstream> metrics;
			if (GPUProfiler::Get().DecodeData(metrics))
			{
				// data can be gathered from profiler
				m_CapturesRemaining--;

				{
					std::ofstream outFile(m_ProfileConfig.OutputFile, std::ofstream::app);

					if (outFile.good())
					{
						// Build all non gpu profiler data that should also be output
						std::stringstream otherData;
						otherData << scene->GetCurrentBrickCount() << "," << scene->GetDemoEditCount() << ",";

						for (const auto& metric : metrics)
						{
							outFile << m_ConfigData << otherData.str() << metric.str();
						}
					}
					else
					{
						LOG_ERROR("Failed to open outfile '{}'", m_ProfileConfig.OutputFile.c_str());
					}
				}

			}
		}
		else
		{
			// Begin a new capture
			PROFILE_CAPTURE_NEXT_FRAME();
		}

	}
	// Check if there are still variations of this demo to perform
	else if (--m_DemoIterationsRemaining > 0)
	{
		// Setup demo for capture
		const DemoConfig& demoConfig = m_ProfileConfig.DemoConfigs[m_DemoConfigIndex];

		// Get current brick size
		float currentBrickSize = 0.0f;
		const UINT iterationsCompleted = demoConfig.IterationCount - m_DemoIterationsRemaining;
		if (demoConfig.IsLinearIncrement)
		{
			currentBrickSize = demoConfig.InitialBrickSize + demoConfig.BrickSizeIncrement * static_cast<float>(iterationsCompleted);
		}
		else
		{
			currentBrickSize = demoConfig.InitialBrickSize * std::pow(demoConfig.BrickSizeMultiplier, static_cast<float>(iterationsCompleted));
		}
		scene->Reset(demoConfig.DemoName, currentBrickSize);

		{
			// Build iteration data string
			std::stringstream stream;
			stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << currentBrickSize << ",";
			m_ConfigData = std::move(stream.str());
		}

		m_CapturesRemaining = m_ProfileConfig.NumCaptures;
	}
	// Current demo config has been completed - progress to the next one
	else if (++m_DemoConfigIndex == m_ProfileConfig.DemoConfigs.size())
	{
		// All demo configs have been captured
		// Process final data
		LOG_INFO("All config profiling completed in {} seconds.", timer->GetTimeSinceReset());

		// Quit application
		Win32Application::ForceQuit();
	}
	else
	{
		// Progress to next demo
		const DemoConfig& demoConfig = m_ProfileConfig.DemoConfigs[m_DemoConfigIndex];

		scene->Reset(demoConfig.DemoName, demoConfig.InitialBrickSize);

		if (const auto orbitalCamera = dynamic_cast<OrbitalCameraController*>(cameraController))
		{
			orbitalCamera->SetOrbitPoint(XMLoadFloat3(&demoConfig.CameraConfig.FocalPoint));
			orbitalCamera->SetOrbitRadius(demoConfig.CameraConfig.OrbitRadius);
		}
		else
		{
			LOG_WARN("Camera is not an orbital camera!");
		}

		{
			// Build iteration data string
			std::stringstream stream;
			stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << demoConfig.InitialBrickSize << ",";
			m_ConfigData = std::move(stream.str());
		}

		m_CapturesRemaining = m_ProfileConfig.NumCaptures;
		m_DemoIterationsRemaining = demoConfig.IterationCount;

		m_DemoBeginTime = timer->GetTimeSinceReset();
		scene->SetPaused(false);
	}
#endif
}

