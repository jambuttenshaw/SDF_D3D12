#pragma once
#include "SDF/SDFEditList.h"


class BaseDemo
{
protected:
	BaseDemo() = default;
public:
	virtual ~BaseDemo() = default;

	DISALLOW_COPY(BaseDemo);
	DISALLOW_MOVE(BaseDemo);

	virtual SDFEditList BuildEditList(float deltaTime) = 0;
	virtual bool DisplayGUI() = 0;

public:
	static void CreateAllDemos();
	static BaseDemo* GetDemoFromName(const std::string& demoName);
private:
	static std::map<std::string, BaseDemo*> s_Demos;
};


class DropsDemo : public BaseDemo
{
	DropsDemo();
public:
	static DropsDemo& Get()
	{
		static DropsDemo instance;
		return instance;
	}

	virtual SDFEditList BuildEditList(float deltaTime) override;
	virtual bool DisplayGUI() override;

private:
	struct SphereData
	{
		XMFLOAT3 scale;
		XMFLOAT3 speed;
		XMFLOAT3 offset;
		float radius;
	};

	UINT m_MaxSphereCount = 1022;
	UINT m_SphereCount = 256;

	std::vector<SphereData> m_Spheres;
	float m_SphereBlend = 0.4f;
	float m_Time = 0.0f;
};


class CubesDemo : public BaseDemo
{
	CubesDemo();
public:
	static CubesDemo& Get()
	{
		static CubesDemo instance;
		return instance;
	}

	virtual SDFEditList BuildEditList(float deltaTime) override;
	virtual bool DisplayGUI() override;

private:
	struct CubeData
	{
		float Scale;
		float DeltaScale;
	};
	std::vector<CubeData> m_CubeScales;

	UINT m_MaxCubeGridSize = 10;
	UINT m_CubeGridSize = 4;

	float m_CubeBlend = 0.0f;

	float m_Time = 0.0f;
};
