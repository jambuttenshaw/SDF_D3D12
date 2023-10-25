#include "pch.h"
#include "GameTimer.h"

GameTimer::GameTimer()
{
	// Calculate seconds per count
	__int64 countsPerSec;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
	m_SecondsPerCount = 1.0 / static_cast<double>(countsPerSec);

	Reset();
}

void GameTimer::Reset()
{
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_CountsOnReset));
	m_LastTickCounts = m_CountsOnReset;
	m_CurrentCounts = m_CountsOnReset;

	m_TimeSinceReset = 0.0;
	m_DeltaTime = 0.0;
}


float GameTimer::Tick()
{
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_CurrentCounts));

	const __int64 deltaCounts = m_CurrentCounts - m_LastTickCounts;
	m_DeltaTime = m_SecondsPerCount * static_cast<double>(deltaCounts);

	// Delta time could potentially be 0 in particular cases
	// where the scheduler switches the main application from one processor to another
	if (m_DeltaTime < 0.0f)
		m_DeltaTime = 0.0f;

	const __int64 countsSinceReset = m_CurrentCounts - m_CountsOnReset;
	m_TimeSinceReset = m_SecondsPerCount * static_cast<double>(countsSinceReset);

	m_FPSFrameCounter++;
	if (m_SecondsPerCount * static_cast<double>(m_CurrentCounts - m_CountsOnLastFPS) >= m_FPSUpdateRate)
	{
		// Update FPS
		m_FPS = static_cast<double>(m_FPSFrameCounter) / m_FPSUpdateRate;

		m_CountsOnLastFPS = m_CurrentCounts;
		m_FPSFrameCounter = 0;
	}

	m_LastTickCounts = m_CurrentCounts;
	return static_cast<float>(m_DeltaTime);
}
