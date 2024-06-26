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

	void SetEditCullingEnabled(bool enabled)
	{
		m_EnableEditCulling = enabled;
		m_BakePipeline = m_EnableEditCulling ? L"Default" : L"NoEditCulling";
	}

private:
	BaseDemo* m_CurrentDemo = nullptr;
	float m_BrickSize = 0.125f;

	std::unique_ptr<SDFObject> m_Geometry;

	bool m_Rebuild = true;
	bool m_AsyncConstruction = false;

	bool m_EnableEditCulling = true;
	std::wstring m_BakePipeline = L"Default";
};
