#pragma once

#include <random>


// Math helper library

// Random number generation
class Random
{
public:
	// generate a random integer between 0 and max
	static int Int(int max);
	// generate a random integer between min and max
	static int Int(int min, int max);

	// generate a random float between 0 and max
	static float Float(float max);
	// generate a random float between min and max
	static float Float(float min, float max);
	// generate a random float between 0 and 1
	static float NormalizedFloat();

private:
	Random();

	inline static Random& Get()
	{
		static Random random;
		return random;
	}

private:
	std::mt19937 m_RandomNumberGenerator;
	std::uniform_real_distribution<float> m_NormalizedFloatDistribution;
};
