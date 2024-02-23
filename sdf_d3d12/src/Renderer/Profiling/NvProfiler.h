#pragma once

#include "Core.h"

#include "Profiler.h"


#ifdef NV_PERF_ENABLE_INSTRUMENTATION

#include <NvPerfCounterConfiguration.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfRangeProfilerD3D12.h>


class D3DQueue;


class NvProfiler : public Profiler
{
public:
	NvProfiler(ID3D12Device* device, const D3DQueue* queue);
	virtual ~NvProfiler() override;

    DISALLOW_COPY(NvProfiler)
	DISALLOW_MOVE(NvProfiler)

	virtual void CaptureNextFrame() override;

    virtual void BeginPass(const char* name) override;
    virtual void EndPass() override;

    virtual void PushRange(const char* name) override;
    virtual void PushRange(const char* name, ID3D12GraphicsCommandList* commandList) override;
    virtual void PopRange() override;
    virtual void PopRange(ID3D12GraphicsCommandList* commandList) override;


private:
    ID3D12Device* m_Device;

    nv::perf::CounterConfiguration m_CounterConfiguration;
    nv::perf::MetricsEvaluator m_MetricsEvaluator;
    std::vector<NVPW_MetricEvalRequest> m_MetricEvalRequests; // This is used in both scheduling and subsequently evaluating the values.

    size_t m_DeviceIndex = 0;

    nv::perf::profiler::RangeProfilerD3D12 m_Profiler;

    bool m_InCollection = false;

    static constexpr double s_NVPerfWarmupTime = 2.0; // Wait 2s to allow the clock to stabilize before beginning to profile.
    static constexpr size_t s_MaxNumRanges = 3;
    static constexpr uint16_t s_NumNestingLevels = 2;

    NVPW_Device_ClockStatus m_ClockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN; // Used to restore clock state when exiting.
    LARGE_INTEGER m_ClockFreq;
    LARGE_INTEGER m_StartTimestamp;

    double m_CurrentRunTime = 0.0;
};

#endif
