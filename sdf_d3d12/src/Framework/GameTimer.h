#pragma once


class GameTimer
{
public:
	GameTimer();

	inline float GetTimeSinceReset() const { return static_cast<float>(m_TimeSinceReset); }	// In seconds
	inline float GetDeltaTime() const { return static_cast<float>(m_DeltaTime); }			// In seconds
	inline float GetFPS() const { return static_cast<float>(m_FPS); }

	void Reset();

	// Returns time since last tick (delta time)
	float Tick();
private:
	double m_SecondsPerCount = 0.0;

	double m_DeltaTime = 0.0;
	double m_TimeSinceReset = 0.0;

	__int64 m_CountsOnReset = 0;
	__int64 m_LastTickCounts = 0;
	__int64 m_CurrentCounts = 0;

	__int64 m_CountsOnLastFPS = 0;
	int m_FPSFrameCounter = 0;
	double m_FPS = 0.0;
	double m_FPSUpdateRate = 1.0; // Recalculate fps every second
};
