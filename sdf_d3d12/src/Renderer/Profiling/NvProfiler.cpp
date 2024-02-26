#include "pch.h"
#include "NvProfiler.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION

#include "Renderer/D3DQueue.h"
#include <nvperf_host_impl.h>

#include <iomanip>


const char* Metrics[] = {
	"sm__throughput.avg.pct_of_peak_sustained_elapsed",
	"lts__average_t_sector_hit_rate_realtime.pct",
	"tpc__sm_rf_registers_allocated_shader_cs_realtime.avg.pct_of_peak_sustained_elapsed",
};


NvProfiler::NvProfiler(const ProfilerArgs& args)
	: Profiler(args)
{
}

NvProfiler::~NvProfiler()
{
	THROW_IF_FALSE(m_Profiler.EndSession(), "Failed to end a session");
	nv::perf::D3D12SetDeviceClockState(m_Device, m_ClockStatus);
}

void NvProfiler::Init(ID3D12Device* device, ID3D12CommandQueue* queue, const ProfilerArgs& args)
{
	m_Device = device;

	LOG_INFO("Creating NV Perf Profiler.");

	THROW_IF_FALSE(nv::perf::InitializeNvPerf(), "Failed to init NvPerf.");
	THROW_IF_FALSE(nv::perf::D3D12LoadDriver(), "Failed to load D3D12 driver.");

	THROW_IF_FALSE(nv::perf::D3D12IsNvidiaDevice(m_Device), "GPU is not an NVidia device.");
	THROW_IF_FALSE(nv::perf::profiler::D3D12IsGpuSupported(m_Device), "GPU is not supported by the profiling API.");

	const size_t deviceIndex = nv::perf::D3D12GetNvperfDeviceIndex(m_Device);
	ASSERT(deviceIndex != (size_t)(~0), "Failed to get device index.");
	m_DeviceIndex = deviceIndex;

	const nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::GetDeviceIdentifiers(deviceIndex);
	ASSERT(deviceIdentifiers.pChipName, "Unrecognized GPU.");

	// Create the metrics evaluator
	{
		std::vector<uint8_t> metricsEvaluatorScratchBuffer;
		const size_t scratchBufferSize = nv::perf::D3D12CalculateMetricsEvaluatorScratchBufferSize(deviceIdentifiers.pChipName);
		ASSERT(scratchBufferSize, "Failed to get scratch buffer size.");
		metricsEvaluatorScratchBuffer.resize(scratchBufferSize);

		NVPW_MetricsEvaluator* pMetricsEvaluator = nv::perf::D3D12CreateMetricsEvaluator(
			metricsEvaluatorScratchBuffer.data(),
			metricsEvaluatorScratchBuffer.size(),
			deviceIdentifiers.pChipName
		);
		ASSERT(pMetricsEvaluator, "Failed to create metrics evaluator");

		m_MetricsEvaluator = nv::perf::MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvaluatorScratchBuffer));
	}

	nv::perf::MetricsConfigBuilder configBuilder;
	{
		NVPA_RawMetricsConfig* pRawMetricsConfig = nv::perf::profiler::D3D12CreateRawMetricsConfig(deviceIdentifiers.pChipName);
		ASSERT(pRawMetricsConfig, "Failed to create the raw metrics config.");

		THROW_IF_FALSE(configBuilder.Initialize(m_MetricsEvaluator, pRawMetricsConfig, deviceIdentifiers.pChipName), "Failed to init config builder.");
	}

	// Add metrics into config builder
	for (size_t i = 0; i < std::size(Metrics); i++)
	{
		const char* const pMetric = Metrics[i];

		NVPW_MetricEvalRequest request{};
		THROW_IF_FALSE(nv::perf::ToMetricEvalRequest(m_MetricsEvaluator, pMetric, request), "Failed to convert metric to metric request.");
		THROW_IF_FALSE(configBuilder.AddMetrics(&request, 1), "Failed to add metric to config.");

		m_MetricEvalRequests.emplace_back(std::move(request));
	}

	// Create counter config
	THROW_IF_FALSE(nv::perf::CreateConfiguration(configBuilder, m_CounterConfiguration), "Failed to create the counter configuration.");

	m_ClockStatus = nv::perf::D3D12GetDeviceClockState(m_Device);
	nv::perf::D3D12SetDeviceClockState(m_Device, NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP);

	{
		nv::perf::profiler::SessionOptions sessionOptions{};
		sessionOptions.maxNumRanges = s_MaxNumRanges;
		THROW_IF_FALSE(m_Profiler.BeginSession(queue, sessionOptions), "Failed to begin session.");
	}

	THROW_IF_FALSE(m_Profiler.EnqueueCounterCollection(m_CounterConfiguration, s_NumNestingLevels), "Failed to enqueue counter collection.");

	LOG_INFO("NV Perf Profiler created successfully");
}



void NvProfiler::CaptureNextFrameImpl()
{
	THROW_IF_FALSE(m_Profiler.EnqueueCounterCollection(m_CounterConfiguration, s_NumNestingLevels), "Failed to enqueue counter collection.");
}


void NvProfiler::BeginPassImpl(const char* name)
{
	if (!m_Profiler.AllPassesSubmitted())
	{
		THROW_IF_FALSE(m_Profiler.BeginPass(), "Failed to begin a pass.");
		PushRangeImpl(name);
	}
}

void NvProfiler::EndPassImpl()
{
	if (!m_Profiler.AllPassesSubmitted() && m_Profiler.IsInPass())
	{
		PopRangeImpl(); // Frame
		THROW_IF_FALSE(m_Profiler.EndPass(), "Failed to end a pass.");

		// Decode counters
		nv::perf::profiler::DecodeResult decodeResult;
		THROW_IF_FALSE(m_Profiler.DecodeCounters(decodeResult), "Failed to decode counters.");
		if (decodeResult.allStatisticalSamplesCollected)
		{
			THROW_IF_FALSE(nv::perf::MetricsEvaluatorSetDeviceAttributes(m_MetricsEvaluator, decodeResult.counterDataImage.data(), decodeResult.counterDataImage.size()), "Failed MetricsEvaluatorSetDeviceAttributes.");

			const size_t numRanges = nv::perf::CounterDataGetNumRanges(decodeResult.counterDataImage.data());
			std::vector<double> metricValues(m_MetricEvalRequests.size());

			for (size_t rangeIndex = 0; rangeIndex < numRanges; rangeIndex++)
			{
				const char* pRangeLeafName = nullptr;
				const std::string rangeFullName = nv::perf::profiler::CounterDataGetRangeName(
					decodeResult.counterDataImage.data(),
					rangeIndex,
					'/',
					&pRangeLeafName
				);

				THROW_IF_FALSE(nv::perf::EvaluateToGpuValues(
					m_MetricsEvaluator,
					decodeResult.counterDataImage.data(),
					decodeResult.counterDataImage.size(),
					rangeIndex,
					m_MetricEvalRequests.size(),
					m_MetricEvalRequests.data(),
					metricValues.data()),
					"Failed EvaluateToGpuValues.");

				// Output data
				std::stringstream stream;
				stream << std::fixed << std::setprecision(0) << rangeFullName;
				for (const auto& metricValue : metricValues)
				{
					stream << ", " << metricValue;
				}
				LOG_INFO("{}", stream.str());
			}

			m_InCollection = false;
		}
	}
}

void NvProfiler::PushRangeImpl(const char* name)
{
	THROW_IF_FALSE(m_Profiler.PushRange(name), "Failed to push a range");
}

void NvProfiler::PushRangeImpl(const char* name, ID3D12GraphicsCommandList* commandList)
{
	THROW_IF_FALSE(nv::perf::profiler::D3D12PushRange(commandList, name), "Failed to push a range");
}

void NvProfiler::PopRangeImpl()
{
	THROW_IF_FALSE(m_Profiler.PopRange(), "Failed to pop a range");
}

void NvProfiler::PopRangeImpl(ID3D12GraphicsCommandList* commandList)
{
	THROW_IF_FALSE(nv::perf::profiler::D3D12PopRange(commandList), "Failed to pop a range");
}

#endif
