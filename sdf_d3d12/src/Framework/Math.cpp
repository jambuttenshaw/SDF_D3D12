#include "pch.h"
#include "Math.h"


Random::Random()
	: m_NormalizedFloatDistribution(0, 1)
{
	std::random_device rd;
	m_RandomNumberGenerator.seed(rd());
}

void Random::Seed(int seed)
{
	Get().m_RandomNumberGenerator.seed(seed);
}

int Random::Int(int max)
{
	return Int(0, max);
}
int Random::Int(int min, int max)
{
	std::uniform_int_distribution<int> dist(min, max);
	return dist(Get().m_RandomNumberGenerator);
}

float Random::Float(float max)
{
	return Float(0, max);
}
float Random::Float(float min, float max)
{
	std::uniform_real_distribution dist(min, max);
	return dist(Get().m_RandomNumberGenerator);
}
float Random::NormalizedFloat()
{
	return Get().m_NormalizedFloatDistribution(Get().m_RandomNumberGenerator);
}
