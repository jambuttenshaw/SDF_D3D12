#pragma once

#include "Scene.h"
#include "SDF/SDFEditList.h"


class Editor : public Scene
{
public:
	Editor(D3DApplication* application);

	void OnUpdate(float deltaTime) override;
	bool DisplayGui() override;

private:
	std::unique_ptr<SDFObject> m_Geometry;
	SDFGeometryInstance* m_GeometryInstance = nullptr;

	float m_BrickSize = 0.125f;

	// The edit list that will be built through user input
	SDFEditList m_EditList;

	// If any changes have occurred that should trigger a rebuild
	bool m_RebuildNext = false;
	bool m_AlwaysRebuild = false;
	bool m_UseAsync = true;

	// Brush parameters
	bool m_ContinuousMode = false;
	float m_ContinuousModeFrequency = 0.05f;

	SDFEdit m_Brush;

	// Brush state
	bool m_UsingBrush = false;
	float m_ContinuousModeTimer = 0.0f;
};
