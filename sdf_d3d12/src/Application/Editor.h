#pragma once

#include "Scene.h"
#include "SDF/SDFEditList.h"


class Editor : public Scene
{
	struct Brush
	{
		std::string Name = "Default";
		SDFEdit BrushEdit;

		bool ContinuousMode = false;
		float ContinuousModeFrequency = 0.05f;
	};

public:
	Editor(D3DApplication* application);

	void OnUpdate(float deltaTime) override;
	bool DisplayGui() override;

private:
	Brush& GetBrush() { return m_Brushes.at(m_CurrentBrush); }
	Brush& GetBrush(size_t i) { return m_Brushes.at(i); }

	bool BrushExistsWithName(const std::string& name) const;

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

	// Brushes
	std::vector<Brush> m_Brushes;
	size_t m_CurrentBrush;

	// Current brush state
	bool m_UsingBrush = false;
	float m_ContinuousModeTimer = 0.0f;
};
