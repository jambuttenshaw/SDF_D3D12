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
	virtual void DisplayGUI() = 0;

	virtual UINT GetEditCount() const = 0;

public:
	static void CreateAllDemos();

	static BaseDemo* GetDemoFromName(const std::string& demoName);
	static const std::map<std::string, BaseDemo*>& GetAllDemos() { return s_Demos; }

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
	virtual void DisplayGUI() override;

	virtual inline UINT GetEditCount() const override { return m_SphereCount + 2; }

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
	virtual void DisplayGUI() override;

	virtual inline UINT GetEditCount() const override { return m_CubeGridSize * m_CubeGridSize * m_CubeGridSize; }

private:
	struct CubeData
	{
		float Scale;
		float DeltaScale;
	};
	std::vector<CubeData> m_CubeScales;

	UINT m_MaxCubeGridSize = 10;
	UINT m_CubeGridSize = 8;

	float m_CubeBlend = 0.1f;

	float m_Time = 0.0f;
};


class RainDemo : public BaseDemo
{
	RainDemo();
public:
	static RainDemo& Get()
	{
		static RainDemo instance;
		return instance;
	}

	virtual SDFEditList BuildEditList(float deltaTime) override;
	virtual void DisplayGUI() override;

	virtual inline UINT GetEditCount() const override { return m_RainDropCount + m_CloudCount + 1; }

private:
	struct RainDropData
	{
		float Mass;
		float Radius;
		float BlendFactor; // To stop drops from instantly affecting blend when they appear

		XMFLOAT3 Position;
		float Velocity; // drops move vertically only
	};
	std::vector<RainDropData> m_RainDrops;

	struct CloudData
	{
		XMFLOAT3 Position;
		float Radius;
		// For animation
		float Frequency;
		float Scale;
		float Offset;
	};
	std::vector<CloudData> m_Clouds;

	float m_Gravity = -40.0f;
	float m_MaxRadius = 0.3f;

	UINT m_CloudCount = 256;
	UINT m_RainDropCount = 256;

	float m_Dimensions = 4.0f;
	float m_CloudHeight = 3.0f;
	float m_FloorHeight = -3.0f;
	
	float m_RainDropBlend = 0.2f;
	float m_CloudBlend = 0.4f;

	float m_Time = 0.0f;
};


class FractalDemo : public BaseDemo
{
	FractalDemo();
public:
	static FractalDemo& Get()
	{
		static FractalDemo instance;
		return instance;
	}

	virtual SDFEditList BuildEditList(float deltaTime) override;
	virtual void DisplayGUI() override;

	virtual inline UINT GetEditCount() const override { return m_FractalGridSize * m_FractalGridSize * m_FractalGridSize; }

private:
	struct FractalData
	{
		XMFLOAT3 Rotation;
		XMFLOAT3 DeltaRotation;
		XMVECTOR Position;
	};
	std::vector<FractalData> m_FractalData;

	UINT m_FractalGridSize = 2;
	float m_Spacing = 3.0f;
	float m_Blending = 0.0f;
};
