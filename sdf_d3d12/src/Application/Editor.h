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
	float m_BrickSize = 0.125f;

	// The edit list that will be built through user input
	SDFEditList m_EditList;

	// If any changes have occurred that should trigger a rebuild
	bool m_RebuildNext = false;
	bool m_AlwaysRebuild = true;
	bool m_UseAsync = false;

	SDFGeometryInstance* m_GeometryInstance = nullptr;


	// Brush parameters
	SDFEdit m_Brush;
};
