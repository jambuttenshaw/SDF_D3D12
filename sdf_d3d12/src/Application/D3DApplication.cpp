#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"


D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
}

void D3DApplication::OnInit()
{
	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight());

	InitImGui();

	// Setup camera

	m_Camera.SetPosition(XMVECTOR{ 0.0f, 1.0f, -5.0f });
	m_Timer.Reset();

	m_CameraController = CameraController{ m_InputManager.get(), &m_Camera };

	// Create SDF factory
	m_SDFFactory = std::make_unique<SDFFactory>();

	// Create an SDF object
	m_SDFObject = std::make_unique<SDFObject>(256, 256, 256);

	m_SDFObject->AddPrimitive(SDFPrimitive::CreateBox(
		{ 0.0f, -0.25f, 0.0f },
		{ 0.4f, 0.4f, 0.4f }));
	m_SDFObject->AddPrimitive(SDFPrimitive::CreateOctahedron({0.0f, -0.25f, 0.0f}, 0.6f));
	m_SDFObject->AddPrimitive(SDFPrimitive::CreateSphere({0.0f, 0.25f, 0.0f}, 0.4f, SDFOperation::Subtraction));

	// Add a torus on top
	Transform torusTransform(0.0f, 0.25f, 0.0f);
	torusTransform.SetPitch(XMConvertToRadians(90.0f));
	m_SDFObject->AddPrimitive(SDFPrimitive::CreateTorus(torusTransform, 0.2f, 0.05f, SDFOperation::SmoothUnion, 0.25f));
	

	// Bake the primitives into the SDF object
	m_SDFFactory->BakeSDFSynchronous(m_SDFObject.get());

	m_GraphicsContext->AddObjectToScene(m_SDFObject.get());


	m_Scene = std::make_unique<Scene>();

	// Build acceleration structure and shader tables
	m_GraphicsContext->BuildShaderTables();
}

void D3DApplication::OnUpdate()
{
	BeginUpdate();

	m_Scene->OnUpdate(m_Timer.GetDeltaTime());

	// Build properties GUI
	ImGui::Begin("Properties");
	ImGui::Separator();
	{
		ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());
	}
	/*
	ImGui::Separator();
	{
		ImGui::Text("Display");

		static const char* s_DisplayModes[] = { "Default", "Display Bounding Box", "Display Normals", "Display Heatmap" };
		int currentMode = static_cast<int>(m_CurrentDisplayMode);
		if (ImGui::Combo("Mode", &currentMode, s_DisplayModes, _countof(s_DisplayModes)))
		{
			m_CurrentDisplayMode = static_cast<DisplayMode>(currentMode);
		}
	}
	*/
	ImGui::Separator();
	{
		ImGui::Text("Camera");
		auto camPos = m_Camera.GetPosition();
		auto camYaw = m_Camera.GetYaw();
		auto camPitch = m_Camera.GetPitch();

		if (ImGui::DragFloat3("Position:", &camPos.x, 0.01f))
			m_Camera.SetPosition(camPos);
		if (ImGui::SliderAngle("Yaw:", &camYaw, -180.0f, 180.0f))
			m_Camera.SetYaw(camYaw);
		if (ImGui::SliderAngle("Pitch:", &camPitch, -89.0f, 89.0f))
			m_Camera.SetPitch(camPitch);

		ImGui::Separator();

		m_CameraController.Gui();

	}

	ImGui::End();

	EndUpdate();
}

void D3DApplication::OnRender()
{
	// Update constant buffer
	m_GraphicsContext->UpdatePassCB(&m_Timer, &m_Camera);
	m_GraphicsContext->UpdateAABBPrimitiveAttributes(*m_Scene);

	// Begin drawing
	m_GraphicsContext->StartDraw();

	// Tell the scene that render is happening
	m_Scene->OnRender();

	// Draw all items
	//m_GraphicsContext->DrawItems(m_Pipelines[m_CurrentDisplayMode].get());
	//m_GraphicsContext->DrawVolume(m_Pipelines[m_CurrentDisplayMode].get(), m_SDFObject->GetSRV());
	m_GraphicsContext->DrawRaytracing(*m_Scene);

	// ImGui Render
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_GraphicsContext->GetCommandList());

	// End draw
	m_GraphicsContext->EndDraw();

	// For multiple ImGui viewports
	const ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, m_GraphicsContext->GetCommandList());
	}

	m_GraphicsContext->Present();
}

void D3DApplication::OnDestroy()
{
	// Release resources used by the graphics context
	m_SDFObject.reset();
	m_SDFFactory.reset();

	// Release graphics context
	m_GraphicsContext.reset();

	// Cleanup ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void D3DApplication::InitImGui() const
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup platform and renderer back-ends
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());
	ImGui_ImplDX12_Init(m_GraphicsContext->GetDevice(),
		m_GraphicsContext->GetBackBufferCount(),
		m_GraphicsContext->GetBackBufferFormat(),
		m_GraphicsContext->GetImGuiResourcesHeap(),
		m_GraphicsContext->GetImGuiCPUDescriptor(),
		m_GraphicsContext->GetImGuiGPUDescriptor());
}


void D3DApplication::BeginUpdate()
{
	const float deltaTime = m_Timer.Tick();
	m_InputManager->Update(deltaTime);

	// Update camera
	m_CameraController.Update(deltaTime);

	// Begin new ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Make viewport dock-able
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void D3DApplication::EndUpdate()
{
	ImGui::Render();
	m_InputManager->EndFrame();
}


void D3DApplication::OnResized()
{
	m_GraphicsContext->Resize(m_Width, m_Height);
}
