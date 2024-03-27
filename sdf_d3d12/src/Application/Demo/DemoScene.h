#pragma once


#include "Demos.h"
#include "Application/D3DApplication.h"
#include "Application/Scene.h"


// This is a scene that will display a demo
class DemoScene : public Scene
{
public:
	DemoScene(D3DApplication* application);
	virtual ~DemoScene() override = default;

	DISALLOW_COPY(DemoScene)
	DEFAULT_MOVE(DemoScene)

	void OnUpdate(float deltaTime) override;
	bool DisplayGui() override;

	// Setting up demo
	void LoadDemo(const std::string& name, float brickSize);

	inline UINT GetCurrentBrickCount() const { return m_Geometry->GetBrickCount(SDFObject::RESOURCES_READ); }
	inline UINT GetCurrentEditCount() const { return m_CurrentDemo->GetEditCount(); }

private:
	BaseDemo* m_CurrentDemo = nullptr;
	float m_BrickSize = 0.1f;

	std::unique_ptr<SDFObject> m_Geometry;

	bool m_Rebuild = true;
	bool m_AsyncConstruction = false;
};
