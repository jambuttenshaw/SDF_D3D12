#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <args.hxx>
#include "pix3.h"


D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
}


// convert wstring to UTF-8 string
#pragma warning( push )
#pragma warning( disable : 4244)
static std::string wstring_to_utf8(const std::wstring& str)
{
	return { str.begin(), str.end() };
}
#pragma warning( pop ) 

bool D3DApplication::ParseCommandLineArgs(LPWSTR argv[], int argc)
{
	// Hacky horrible wstring to string conversion
	// because the args library doesn't support wstring
	// and windows only gives wstring
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++)
	{
		args.emplace_back(wstring_to_utf8(std::wstring(argv[i])));
	}

	args::ArgumentParser parser("SDF Rendering in DirectX Raytracing");
	args::HelpFlag help(parser, "help", "Display this help menu", { "help" });

	args::Group windowFlags(parser, "Window Flags");
	args::ValueFlag<int> width(windowFlags, "Width", "Set the window width", { 'w' });
	args::ValueFlag<int> height(windowFlags, "Height", "Set the window height", { 'h' });
	args::Flag fullscreen(windowFlags, "Fullscreen", "Start application in fullscreen", { 'f', "fullscreen" });
	args::Flag noGUI(windowFlags, "No GUI", "Disable GUI", { "no-gui" });

	args::Group graphicsContextFlags(parser, "Graphics Context Flags");
	args::Flag enablePIX(graphicsContextFlags, "Enable GPU Captures", "Enable PIX GPU Captures", { "enable-gpu-capture" });
	args::Flag enableDRED(graphicsContextFlags, "Enable DRED", "Enable Device Removal Extended Data", { "enable-dred" });

	try
	{
		parser.ParseCLI(args);
	}
	catch (const args::Help&)
	{
		LOG_INFO(parser.Help());
		return false;
	}
	catch (const args::ParseError& e)
	{
		LOG_ERROR(e.what());
		return false;
	}

	if (width)
		m_Width = width.Get();
	if (height)
		m_Height = height.Get();
	if (fullscreen)
		m_ToggleFullscreen = true;
	if (noGUI)
		m_DisableGUI = true;

	if (enablePIX)
		m_GraphicsContextFlags.EnableGPUCaptures = true;
	if (enableDRED)
		m_GraphicsContextFlags.EnableDRED = true;

	return true;
}


void D3DApplication::OnInit()
{
	LOG_INFO("Application startup.");

	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight(), m_GraphicsContextFlags);

	InitImGui();

	// Setup camera

	m_Camera.SetPosition(XMVECTOR{ 0.0f, 3.0f, -8.0f });
	m_Timer.Reset();

	m_CameraController = CameraController{ m_InputManager.get(), &m_Camera };

	m_Scene = std::make_unique<Scene>();

	m_Raytracer = std::make_unique<Raytracer>();
	m_Raytracer->Setup(*m_Scene);

	LOG_INFO("Application startup complete.");
}

void D3DApplication::OnUpdate()
{
	if (m_ToggleFullscreen)
	{
		m_ToggleFullscreen = false;
		Win32Application::ToggleFullscreenWindow(m_GraphicsContext->GetSwapChain());
	}

	PIXBeginEvent(PIX_COLOR_INDEX(0), L"App Update");
	BeginUpdate();

	m_Scene->OnUpdate(m_Timer.GetDeltaTime());

	if (m_ShowMainMenuBar && !m_DisableGUI)
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Application Info", nullptr, &m_ShowApplicationInfo);
				ImGui::MenuItem("Scene Info", nullptr, &m_ShowSceneInfo);
				ImGui::Separator();
				ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowImGuiDemo);
				ImGui::Separator();
				ImGui::MenuItem("Show Menu", nullptr, &m_ShowMainMenuBar);
	
				ImGui::EndMenu();
			}

			m_Scene->ImGuiSceneMenu();

			ImGui::EndMainMenuBar();
		}
		if (m_ShowImGuiDemo)
			ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
		if (m_ShowApplicationInfo)
			m_ShowApplicationInfo = ImGuiApplicationInfo();
		if (m_ShowSceneInfo)
			m_ShowSceneInfo = m_Scene->ImGuiSceneInfo();
	}

	EndUpdate();
	PIXEndEvent();
}

void D3DApplication::OnRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(1), L"App Render");

	// Update constant buffer
	m_GraphicsContext->UpdatePassCB(&m_Timer, &m_Camera, m_RenderFlags, m_HeatmapQuantization, m_HeatmapHueRange);

	// Begin drawing
	m_GraphicsContext->StartDraw();

	// Tell the scene that render is happening
	// This will update acceleration structures and other things to render the scene
	m_Scene->PreRender();

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
	PIXEndEvent();
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

	style.ScaleAllSizes(1.5f);

	const auto font = io.Fonts->AddFontFromFileTTF("assets/fonts/Cousine-Regular.ttf", 18);
	io.FontDefault = font;

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

	if (m_InputManager->IsKeyPressed(KEY_F11))
		m_ShowMainMenuBar = true;

	// Update camera
	m_CameraController.Update(deltaTime);

	SetCustomWindowText((std::wstring(L"FPS: ") + std::to_wstring(static_cast<int>(m_Timer.GetFPS()))).c_str());

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


bool D3DApplication::ImGuiApplicationInfo()
{
	bool open = true;
	if (ImGui::Begin("Application", &open))
	{
		ImGui::Separator();
		{
			ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());

			bool vsync = m_GraphicsContext->GetVSyncEnabled();
			if (ImGui::Checkbox("VSync", &vsync))
				m_GraphicsContext->SetVSyncEnabled(vsync);
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
			auto RenderFlagOption = [&](const char* name, UINT value)-> bool
				{
					bool flag = m_RenderFlags & value;
					if (ImGui::Checkbox(name, &flag))
					{
						if (flag)
							m_RenderFlags |= value;
						else
							m_RenderFlags &= ~value;
						return true;
					}
					return false;
				};

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
			}
			RenderFlagOption("Edit Count", RENDER_FLAG_DISPLAY_BRICK_EDIT_COUNT);
		}

		ImGui::Separator();
		{
			const char* samplers[] = { "Point", "Linear" };
			int currentSampler = m_Raytracer->GetCurrentSampler();
			if (ImGui::Combo("Sampler", &currentSampler, samplers, ARRAYSIZE(samplers)))
			{
				m_Raytracer->SetCurrentSampler(static_cast<Raytracer::GlobalSamplerType>(currentSampler));
			}
		}

		ImGui::Separator();

		ImGui::Text("Heatmap");

		int heatmap = static_cast<int>(m_HeatmapQuantization);
		if (ImGui::InputInt("Quantization", &heatmap))
		{
			m_HeatmapQuantization = static_cast<UINT>(heatmap);
		}
		ImGui::SliderFloat("Hue Range", &m_HeatmapHueRange, 0.0f, 1.0f);

		ImGui::End();
	}
	return open;
}



void D3DApplication::OnResized()
{
	m_GraphicsContext->Resize(m_Width, m_Height);
	m_Raytracer->Resize();
}


bool D3DApplication::GetTearingSupport() const
{
	return g_D3DGraphicsContext && g_D3DGraphicsContext->GetTearingSupport();
}
IDXGISwapChain* D3DApplication::GetSwapChain() const
{
	if (g_D3DGraphicsContext)
		return g_D3DGraphicsContext->GetSwapChain();
	return nullptr;
}
