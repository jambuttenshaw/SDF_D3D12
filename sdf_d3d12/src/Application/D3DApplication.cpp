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
	LOG_INFO("Application startup.");

	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight());

	InitImGui();

	// Setup camera

	m_Camera.SetPosition(XMVECTOR{ 0.0f, 1.0f, -5.0f });
	m_Timer.Reset();

	m_CameraController = CameraController{ m_InputManager.get(), &m_Camera };

	m_Scene = std::make_unique<Scene>();

	m_Raytracer = std::make_unique<Raytracer>();
	m_Raytracer->Setup(*m_Scene);

	LOG_INFO("Application startup complete.");
}

void D3DApplication::OnUpdate()
{
	BeginUpdate();

	m_Scene->OnUpdate(m_Timer.GetDeltaTime());

	// Build properties GUI
	ImGui::Begin("Application");
	ImGui::Separator();
	{
		ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());
	}
	ImGui::Separator();
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Camera");
		ImGui::PopStyleColor();

		ImGui::Separator();

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
	ImGui::Separator();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Renderer");
	ImGui::PopStyleColor();

	ImGui::Separator();
	{
		auto RenderFlagOption = [&](const char* name, UINT value)
			{
				bool flag = m_RenderFlags & value;
				if (ImGui::Checkbox(name, &flag))
				{
					if (flag)
						m_RenderFlags |= value;
					else
						m_RenderFlags &= ~value;
				}};
		ImGui::Text("Display Options");
		RenderFlagOption("Bounding Box", RENDER_FLAG_DISPLAY_BOUNDING_BOX);
		if (m_RenderFlags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
		{
			RenderFlagOption("Brick Index", RENDER_FLAG_DISPLAY_BRICK_INDEX);
			RenderFlagOption("Pool UVW", RENDER_FLAG_DISPLAY_POOL_UVW);
		}
		else
		{
			RenderFlagOption("Heatmap", RENDER_FLAG_DISPLAY_HEATMAP);
			RenderFlagOption("Normals", RENDER_FLAG_DISPLAY_NORMALS);
			RenderFlagOption("Iteration Guard Terminations", RENDER_FLAG_DISPLAY_ITERATION_GUARD_TERMINATIONS);
		}
	}

	ImGui::End();

	EndUpdate();
}

void D3DApplication::OnRender()
{
	// Update constant buffer
	m_GraphicsContext->UpdatePassCB(&m_Timer, &m_Camera, m_RenderFlags);

	// Begin drawing
	m_GraphicsContext->StartDraw();

	// Tell the scene that render is happening
	// This will update acceleration structures and other things to render the scene
	m_Scene->OnRender();

	// Perform raytracing
	m_Raytracer->DoRaytracing();
	m_GraphicsContext->CopyRaytracingOutput(m_Raytracer->GetRaytracingOutput());

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
	m_GraphicsContext->WaitForGPUIdle();

	m_Scene.reset();
	m_Raytracer.reset();

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
	m_Raytracer->Resize();
}
