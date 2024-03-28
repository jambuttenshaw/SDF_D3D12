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
	m_Brush = SDFEdit::CreateSphere({}, 0.25f, SDF_OP_SMOOTH_UNION, 1.0f);
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

			edit.Validate();

			m_EditList.AddEdit(edit);
			m_RebuildNext = true;
		}
	}


	if (m_RebuildNext || m_AlwaysRebuild)
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

	auto addTitle = [](const char* title)
	{
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
			ImGui::Text(title);
			ImGui::PopStyleColor();
			ImGui::Separator();
	};

	addTitle("General");

	ImGui::Checkbox("Always Rebuild", &m_AlwaysRebuild);
	ImGui::Checkbox("Use Async", &m_UseAsync);

	addTitle("Geometry");

	if (ImGui::SliderFloat("Brick Size", &m_BrickSize, 0.0625f, 1.0f))
	{
		m_Geometry->SetNextRebuildBrickSize(m_BrickSize);
		m_RebuildNext = true;
	}

	if (ImGui::Button("Clear"))
	{
		m_EditList.Reset();
		m_EditList.AddEdit(SDFEdit::CreateSphere({}, 0.5f));

		m_RebuildNext = true;
	}

	addTitle("Materials");

	auto setMatSlot = [this](const char* slotLabel, UINT slotIndex) -> bool
		{
			int mat = static_cast<int>(this->m_Geometry->GetMaterialID(slotIndex));
			if (ImGui::InputInt(slotLabel, &mat))
			{
				MaterialManager* materialManager = this->m_Application->GetMaterialManager();
				m_Geometry->SetMaterial(materialManager->GetMaterial(static_cast<UINT>(mat)), slotIndex);
				return true;
			}
			return false;
		};

	m_RebuildNext |= setMatSlot("Slot 0", 0);
	m_RebuildNext |= setMatSlot("Slot 1", 1);
	m_RebuildNext |= setMatSlot("Slot 2", 2);
	m_RebuildNext |= setMatSlot("Slot 3", 3);

	addTitle("Brush");

	m_Brush.DrawGui();

	ImGui::End();

	// Don't close GUI
	return true;
}
