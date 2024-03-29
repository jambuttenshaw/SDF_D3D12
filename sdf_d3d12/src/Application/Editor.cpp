#include "pch.h"
#include "Editor.h"

#include "D3DApplication.h"

#include "Framework/GuiHelpers.h"
#include "Input/InputManager.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"


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
	m_Brushes.emplace_back();
	m_Brushes.back().BrushEdit = SDFEdit::CreateSphere({}, 0.25f, SDF_OP_SMOOTH_UNION, 0.1f);

	m_CurrentBrush = 0;

	// Disable mouse capture on the camera controller
	m_Application->GetCameraController()->SetAllowMouseCapture(false);
}


void Editor::OnUpdate(float deltaTime)
{
	const InputManager* inputManager = m_Application->GetInputManager();
	const Picker* picker = m_Application->GetPicker();

	if (m_UsingBrush && GetBrush().ContinuousMode)
	{
		m_ContinuousModeTimer += deltaTime;
	}

	if (inputManager->IsKeyDown(KEY_LBUTTON) && !m_UsingBrush)
	{
		// Get brush location
		const auto& hit = picker->GetLastPick();
		if (hit.instanceID == m_GeometryInstance->GetInstanceID())
		{
			// Hit is valid
			SDFEdit edit = GetBrush().BrushEdit;
			edit.PrimitiveTransform.SetTranslation(hit.hitLocation);

			edit.Validate();

			m_EditList.AddEdit(edit);
			m_RebuildNext = true;

			m_UsingBrush = true;
			m_ContinuousModeTimer = 0.0f;
		}
	}

	if (inputManager->IsKeyReleased(KEY_LBUTTON) || m_ContinuousModeTimer > GetBrush().ContinuousModeFrequency)
	{
		m_UsingBrush = false;
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
	ImGui::ShowDemoWindow();

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

	ImGui::Separator();
	{
		GuiHelpers::DisableScope disable(m_EditList.GetEditCount() == 0);

		if (ImGui::Button("Undo", ImVec2(-FLT_MIN, 0)))
		{
			m_EditList.PopEdit();
			m_RebuildNext = true;
		}
	}

	addTitle("Geometry");

	if (ImGui::SliderFloat("Brick Size", &m_BrickSize, 0.0625f, 1.0f))
	{
		m_Geometry->SetNextRebuildBrickSize(m_BrickSize);
		m_RebuildNext = true;
	}

	float evalRange = m_EditList.GetEvaluationRange();
	if (ImGui::DragFloat("Eval Range", &evalRange, 0.01f))
	{
		if (evalRange < 1.0f)
			evalRange = 1.0f;

		m_EditList.SetEvaluationRange(evalRange);
		m_RebuildNext = true;
	}

	addTitle("Edits");

	if (ImGui::Button("Clear All", ImVec2(-FLT_MIN, 0.0f)))
	{
		m_EditList.Reset();
		m_EditList.AddEdit(SDFEdit::CreateSphere({}, 0.5f));

		m_RebuildNext = true;
	}

	{	// Display how many edits have been used
		const float editsUsed = m_EditList.GetEditCount() / 1024.0f;
		ImGui::Text("Edits Used:");
		ImGui::SameLine();
		ImGui::ProgressBar(editsUsed, ImVec2(-FLT_MIN, 0.0f));
	}

	addTitle("Materials");

	auto setMatSlot = [this](const char* slotLabel, UINT slotIndex) -> bool
		{
			int mat = static_cast<int>(this->m_Geometry->GetMaterialID(slotIndex));
			MaterialManager* materialManager = this->m_Application->GetMaterialManager();

			if (materialManager->DrawMaterialComboGui(slotLabel, mat))
			{
				m_Geometry->SetMaterial(materialManager->GetMaterial(static_cast<UINT>(mat)), slotIndex);
				return true;
			}
			return false;
		};

	m_RebuildNext |= setMatSlot("Slot 0", 0);
	m_RebuildNext |= setMatSlot("Slot 1", 1);
	m_RebuildNext |= setMatSlot("Slot 2", 2);
	m_RebuildNext |= setMatSlot("Slot 3", 3);

	addTitle("Brush Palette");

	if (ImGui::Button("New Brush", ImVec2(-FLT_MIN, 0.0f)))
	{
		Brush newBrush = GetBrush();

		UINT i = 0;
		do
		{
			newBrush.Name = "Brush " + std::to_string(i++);
		} while (BrushExistsWithName(newBrush.Name));

		m_CurrentBrush = m_Brushes.size();
		m_Brushes.emplace_back(std::move(newBrush));
	}

	ImGui::Separator();
	ImGui::Text("Selected Brush");
	ImGui::Separator();

	for (size_t i = 0; i < m_Brushes.size(); i++)
	{
		Brush& brush = GetBrush(i);
		if (ImGui::Selectable(brush.Name.c_str(), brush.Name == GetBrush().Name))
		{
			m_CurrentBrush = i;
		}
	}

	addTitle("Brush Settings");

	std::string name = GetBrush().Name;
	if (ImGui::InputText("Name", &name))
	{
		if (!BrushExistsWithName(name) && name.length() > 0)
		{
			GetBrush().Name = std::move(name);
		}
	}

	GetBrush().BrushEdit.DrawGui();

	ImGui::Separator();

	ImGui::Checkbox("Continuous Mode", &GetBrush().ContinuousMode);
	{
		GuiHelpers::DisableScope disable(!GetBrush().ContinuousMode);
		ImGui::DragFloat("Continuous Mode Freq", &GetBrush().ContinuousModeFrequency, 0.001f);
	}

	ImGui::End();

	// Don't close GUI
	return true;
}


bool Editor::BrushExistsWithName(const std::string& name) const
{
	for (const auto& brush : m_Brushes)
	{
		if (brush.Name == name)
			return true;
	}
	return false;
}

