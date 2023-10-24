#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/RenderItem.h"


D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
}

void D3DApplication::OnInit()
{
	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight());

	InitImGui();

	m_Camera.SetPosition({ 0.0f, 0.0f, -5.0f });
	m_Timer.Reset();

	m_Cube = m_GraphicsContext->CreateRenderItem();
	ASSERT(m_Cube, "Failed to create cube");
}

void D3DApplication::OnUpdate()
{
	float deltaTime = m_Timer.Tick();

	// Update objects
	m_CubeRotation += deltaTime;
	XMMATRIX m = XMMatrixRotationAxis({ 0.77f, 0.77f, 0.0 }, m_CubeRotation);
	m_Cube->SetWorldMatrix(m);

	// Begin new ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Make viewport dock-able
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// Build properties GUI
	ImGui::Begin("Properties");
	ImGui::Separator();
	{
		ImGui::Text("Timer");
		ImGui::LabelText("Time", "%.2f", m_Timer.GetTimeSinceReset());
		ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());
		ImGui::Separator();
	}

	{
		ImGui::Text("Camera");
		auto camPos = m_Camera.GetPosition();
		auto camYaw = m_Camera.GetYaw();
		auto camPitch = m_Camera.GetPitch();

		if (ImGui::DragFloat3("Position:", &camPos.x, 0.01f))
			m_Camera.SetPosition(camPos);
		if (ImGui::SliderAngle("Yaw:", &camYaw, -180.0f, 180.0f))
			m_Camera.SetYaw(camYaw);
		if (ImGui::SliderAngle("Pitch:", &camPitch, -89.9f, 89.9f))
			m_Camera.SetPitch(camPitch);

		ImGui::Separator();
	}

	ImGui::End();

	ImGui::Render();
}

void D3DApplication::OnRender()
{
	// Update constant buffers
	m_GraphicsContext->UpdateObjectCBs();
	m_GraphicsContext->UpdatePassCB(&m_Timer, &m_Camera);

	// Begin drawing
	m_GraphicsContext->StartDraw();

	// Draw all items
	m_GraphicsContext->DrawItems();

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


void D3DApplication::OnResized()
{
	m_GraphicsContext->Resize(m_Width, m_Height);
}
