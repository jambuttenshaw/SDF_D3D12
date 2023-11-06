#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "Framework/RenderItem.h"

#include "SDF/SDFTypes.h"


D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
}

void D3DApplication::OnInit()
{
	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight());

	InitImGui();

	// Create pipelines
	LOG_TRACE("Creating compute pipelines...");

	// Create a root signature consisting of two root descriptors for CBVs (per-object and per-pass)
	CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
	CD3DX12_ROOT_PARAMETER1 rootParameters[3];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);	// per-object cb
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);	// per-pass cb
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);	// output uav

	rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL);

	D3D_SHADER_MACRO defaultDefines[] = { { nullptr, nullptr } };
	D3D_SHADER_MACRO disableShadowDefines[] = { { "DISABLE_SHADOW", nullptr }, { nullptr, nullptr } };
	D3D_SHADER_MACRO disableLightDefines[] = { { "DISABLE_LIGHT", nullptr }, { nullptr, nullptr } };
	D3D_SHADER_MACRO heatmapDefines[] = { { "DISPLAY_HEATMAP", nullptr }, { nullptr, nullptr } };

	D3DComputePipelineDesc desc = {};
	desc.NumRootParameters = 3;
	desc.RootParameters = rootParameters;
	desc.Shader = L"assets/shaders/compute.hlsl";
	desc.EntryPoint = "main";

	desc.Defines = defaultDefines;
	m_Pipelines.insert(std::make_pair(DisplayMode::Default, std::make_unique<D3DComputePipeline>(&desc)));

	desc.Defines = disableShadowDefines;
	m_Pipelines.insert(std::make_pair(DisplayMode::DisableShadow, std::make_unique<D3DComputePipeline>(&desc)));

	desc.Defines = disableLightDefines;
	m_Pipelines.insert(std::make_pair(DisplayMode::DisableLight, std::make_unique<D3DComputePipeline>(&desc)));

	desc.Defines = heatmapDefines;
	m_Pipelines.insert(std::make_pair(DisplayMode::Heatmap, std::make_unique<D3DComputePipeline>(&desc)));

	LOG_TRACE("Compute pipelines created.");


	// Setup camera

	m_Camera.SetPosition(XMVECTOR{ 0.0f, 1.0f, -5.0f });
	m_Timer.Reset();

	m_CameraController = CameraController{ m_InputManager.get(), &m_Camera };


	// Create scene items
	m_Cube = m_GraphicsContext->CreateRenderItem();

	const auto boxPrimitive = SDFPrimitive::CreateBox({ 0.5f, 0.5f, 0.5f }, SDFOperation::Union, 0.0f, XMFLOAT4{ 0.3f, 0.8f, 0.2f, 1.0f });
	m_Cube->SetSDFPrimitiveData(boxPrimitive);
	m_Cube->SetTranslation({ 0.0f, 1.0f, 1.5f });


	m_Octahedron = m_GraphicsContext->CreateRenderItem();
	m_Octahedron->SetSDFPrimitiveData(
		SDFPrimitive::CreateOctahedron(1.0f, SDFOperation::Union, 0.0f, XMFLOAT4{ 0.92f, 0.2f, 0.2f, 1.0f })
	);
	m_Octahedron->SetTranslation({ 2.0f, 0.3f, 0.0f });


	m_Torus = m_GraphicsContext->CreateRenderItem();
	m_Torus->SetSDFPrimitiveData(
		SDFPrimitive::CreateTorus(0.8f, 0.3f, SDFOperation::SmoothUnion, 0.3f, XMFLOAT4{ 0.2f, 0.58f, 0.92f, 1.0f })
	);
	m_Torus->SetTranslation({ 0.3f, 0.0f, 0.4f });
	m_Torus->SetPitch(XMConvertToRadians(-30.0f));


	m_Plane = m_GraphicsContext->CreateRenderItem();
	m_Plane->SetSDFPrimitiveData(
		SDFPrimitive::CreatePlane({ 0.0f, 1.0f, 0.0f }, 1.0f, SDFOperation::Union, 0.0f, XMFLOAT4{ 0.13f, 0.2f, 0.17f, 1.0f })
	);

	m_SDFFactory = std::make_unique<SDFFactory>();

	m_SDFObject = std::make_unique<SDFObject>(256, 256, 256);
	m_SDFFactory->BakeSDFSynchronous(m_SDFObject.get());
}

void D3DApplication::OnUpdate()
{
	BeginUpdate();

	// Build properties GUI
	ImGui::Begin("Properties");
	ImGui::Separator();
	{
		ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());
	}
	ImGui::Separator();
	{
		ImGui::Text("Display");

		static const char* s_DisplayModes[] = { "Default", "Disable Shadow", "Disable Lighting", "Heatmap" };
		int currentMode = static_cast<int>(m_CurrentDisplayMode);
		if (ImGui::Combo("Mode", &currentMode, s_DisplayModes, _countof(s_DisplayModes)))
		{
			m_CurrentDisplayMode = static_cast<DisplayMode>(currentMode);
		}
	}
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
	ImGui::Separator();
	{
		ImGui::Text("Torus");

		m_Torus->DrawGui();

	}
	ImGui::Separator();

	ImGui::End();

	EndUpdate();
}

void D3DApplication::OnRender()
{
	// Update constant buffers
	m_GraphicsContext->UpdateObjectCBs();
	m_GraphicsContext->UpdatePassCB(&m_Timer, &m_Camera);

	// Begin drawing
	m_GraphicsContext->StartDraw();

	// Draw all items
	m_GraphicsContext->DrawItems(m_Pipelines[m_CurrentDisplayMode].get());

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
