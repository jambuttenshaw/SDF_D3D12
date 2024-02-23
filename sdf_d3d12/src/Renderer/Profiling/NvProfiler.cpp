#include "pch.h"
#include "NvProfiler.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION

#include <nvperf_host_impl.h>
#include <NvPerfD3D12.h>

#include "Renderer/D3DQueue.h"


const char* Metrics[] = {
	"gpu__time_duration.sum",
};


NvProfiler::NvProfiler(ID3D12Device* device, const D3DQueue* queue)
	: m_Device(device)
{
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
		THROW_IF_FALSE(m_Profiler.BeginSession(queue->GetCommandQueue(), sessionOptions), "Failed to begin session.");
	}

	QueryPerformanceFrequency(&m_ClockFreq);
	QueryPerformanceCounter(&m_StartTimestamp);

	LOG_INFO("NV Perf Profiler created successfully");
}

NvProfiler::~NvProfiler()
{
	THROW_IF_FALSE(m_Profiler.EndSession(), "Failed to end a session");
	nv::perf::D3D12SetDeviceClockState(m_Device, m_ClockStatus);
}


void NvProfiler::BeginPass()
{
	if (!m_Profiler.AllPassesSubmitted())
	{
		THROW_IF_FALSE(m_Profiler.BeginPass(), "Failed to begin a pass.");
	}
}

void NvProfiler::EndPass()
{
	if (!m_Profiler.AllPassesSubmitted() && m_Profiler.IsInPass())
	{
		THROW_IF_FALSE(m_Profiler.EndPass(), "Failed to end a pass.");
	}
}

void NvProfiler::PushRange(const char* name)
{
	THROW_IF_FALSE(m_Profiler.PushRange(name), "Failed to push a range");
}

void NvProfiler::PopRange()
{
	THROW_IF_FALSE(m_Profiler.PopRange(), "Failed to pop a range");
}


#endif
