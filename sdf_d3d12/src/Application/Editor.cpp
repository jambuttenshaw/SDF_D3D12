#include "pch.h"
#include "Editor.h"

#include "D3DApplication.h"
#include "imgui.h"
#include "Input/InputManager.h"


Editor::Editor(D3DApplication* application)
	: Scene(application, 1)
	, m_EditList(1024, 4.0f)
{
	m_Geometry = std::make_unique<SDFObject>(m_BrickSize, 500'000);


	// Some geometry is needed in the edit list to begin with
	m_EditList.AddEdit(SDFEdit::CreateSphere({}, 0.5f));

	// Bake geometry to start with
	m_Application->GetSDFFactory()->BakeSDFSync(L"Default", m_Geometry.get(), m_EditList);
	m_Geometry->FlipResources();


	// Populate scene
	AddGeometry(L"Geometry", m_Geometry.get());
	// Create a geometry instance
	m_GeometryInstance = CreateGeometryInstance(L"Geometry");


	// Setup brush
	m_Brush = SDFEdit::CreateSphere({}, 0.25f);
}


void Editor::OnUpdate(float deltaTime)
{
	const InputManager* inputManager = m_Application->GetInputManager();
	const Picker* picker = m_Application->GetPicker();

	if (inputManager->IsKeyPressed(KEY_LBUTTON))
	{
		// Get brush location
		const auto& hit = picker->GetLastPick();
		if (hit.instanceID == m_GeometryInstance->GetInstanceID())
		{
			// Hit is valid
			SDFEdit edit = m_Brush;
			edit.PrimitiveTransform.SetTranslation(hit.hitLocation);

			m_EditList.AddEdit(edit);
			m_RebuildNext = true;
		}
	}


	if (m_RebuildNext)
	{
		// Don't rebuild with an empty edit list
		if (m_EditList.GetEditCount() > 0)
		{
			SDFFactoryHierarchicalAsync* factory = m_Application->GetSDFFactory();
			if (m_UseAsync)
				factory->BakeSDFAsync(L"Default", m_Geometry.get(), m_EditList);
			else
				factory->BakeSDFSync(L"Default", m_Geometry.get(), m_EditList);

			m_RebuildNext = false;
		}
	}
}


bool Editor::DisplayGui()
{
	ImGui::Begin("Editor");

	if (ImGui::Button("Clear"))
	{
		m_EditList.Reset();
		m_EditList.AddEdit(SDFEdit::CreateSphere({}, 0.5f));

		m_RebuildNext = true;
	}

	ImGui::Separator();

	m_Brush.DrawGui();

	ImGui::End();

	// Don't close GUI
	return true;
}


