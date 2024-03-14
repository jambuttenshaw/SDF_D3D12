#pragma once

#include <random>

using namespace DirectX;

// Math helper library

// Random number generation
class Random
{
public:
	static void Seed(int seed);

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


// Math utility functions
class Math
{
public:
	template <typename T>
	inline static int Sign(T val) {
		return (T(0) < val) - (val < T(0));
	}
};
