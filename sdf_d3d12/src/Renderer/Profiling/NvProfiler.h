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
	NvProfiler(ProfilerQueue queue);
	virtual ~NvProfiler() override;

    DISALLOW_COPY(NvProfiler)
	DISALLOW_MOVE(NvProfiler)

protected:
    virtual void Init(ID3D12Device* device, ID3D12CommandQueue* queue) override;

	virtual void CaptureNextFrameImpl() override;

    virtual void BeginPassImpl(const char* name) override;
    virtual void EndPassImpl() override;

    virtual void PushRangeImpl(const char* name) override;
    virtual void PushRangeImpl(const char* name, ID3D12GraphicsCommandList* commandList) override;
    virtual void PopRangeImpl() override;
    virtual void PopRangeImpl(ID3D12GraphicsCommandList* commandList) override;

private:
    ID3D12Device* m_Device = nullptr;

    nv::perf::CounterConfiguration m_CounterConfiguration;
    nv::perf::MetricsEvaluator m_MetricsEvaluator;
    std::vector<NVPW_MetricEvalRequest> m_MetricEvalRequests; // This is used in both scheduling and subsequently evaluating the values.

    size_t m_DeviceIndex = 0;

    nv::perf::profiler::RangeProfilerD3D12 m_Profiler;

    NVPW_Device_ClockStatus m_ClockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN; // Used to restore clock state when exiting.
};

#endif
