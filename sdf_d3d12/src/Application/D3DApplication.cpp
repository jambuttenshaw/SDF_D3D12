#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <imgui_internal.h>

#include "Renderer/Profiling/GPUProfiler.h"

#include <args.hxx>

#include "Demos.h"
#include "pix3.h"

#include <fstream>
#include <iomanip>

#include "Framework/PickingQueryInterface.h"
#include "Renderer/D3DDebugTools.h"



D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
	m_ProfilingDataCollector = std::make_unique<ProfilingDataCollector>();
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
	args::Flag fullscreen(windowFlags, "Fullscreen", "Start application in fullscreen", { 'f', "fullscreen" });
	args::Flag noGUI(windowFlags, "No GUI", "Disable GUI", { "no-gui" });

	args::Group graphicsContextFlags(parser, "Graphics Context Flags");
	args::Flag enablePIX(graphicsContextFlags, "Enable GPU Captures", "Enable PIX GPU Captures", { "enable-gpu-capture" });
	args::Flag enableDRED(graphicsContextFlags, "Enable DRED", "Enable Device Removal Extended Data", { "enable-dred" });

	args::Group applicationFlags(parser, "Application Flags");
	args::Flag orbitalCamera(applicationFlags, "Orbital Camera", "Enable an orbital camera", { "orbital-camera" });

#ifdef ENABLE_INSTRUMENTATION
	// These settings won't do anything in a non-instrumented build
	args::Command profilingCommand(parser, "profile", "Launch application in profiling mode", [this](args::Subparser& subparser)
		{
			this->m_ProfilingDataCollector->ParseCommandLineArgs(subparser);
		});
#endif

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
	catch (const args::ValidationError& e)
	{
		LOG_ERROR(e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		return false;
	}

	if (fullscreen)
		m_ToggleFullscreen = true;
	if (noGUI)
		m_DisableGUI = true;

	if (enablePIX)
		m_GraphicsContextFlags.EnableGPUCaptures = true;
	if (enableDRED)
		m_GraphicsContextFlags.EnableDRED = true;

	if (orbitalCamera)
		m_UseOrbitalCamera = true;

	return true;
}


void D3DApplication::OnInit()
{
	LOG_INFO("Application startup.");

	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight(), m_GraphicsContextFlags);

	m_TextureLoader = std::make_unique<TextureLoader>();

	// Setup camera
	m_Camera.SetPosition(XMVECTOR{ 0.0f, 3.0f, -8.0f });
	m_Timer.Reset();

	if (m_UseOrbitalCamera)
		m_CameraController = std::make_unique<OrbitalCameraController>(m_InputManager.get(), &m_Camera);
	else
		m_CameraController = std::make_unique<CameraController>(m_InputManager.get(), &m_Camera);

	InitImGui();

	m_Raytracer = std::make_unique<Raytracer>();
	m_PickingQueryInterface = std::make_unique<PickingQueryInterface>();

	m_LightManager = std::make_unique<LightManager>();
	m_MaterialManager = std::make_unique<MaterialManager>(4);

	m_Factory = std::make_unique<SDFFactoryHierarchicalAsync>();
	m_Geometry = std::make_unique<SDFObject>(0.1f, 500'000);

	BaseDemo::CreateAllDemos();
	if (m_ProfilingDataCollector->InitProfiler())
	{
		// Force various modes for profiling
		if (!m_UseOrbitalCamera)
		{
			m_UseOrbitalCamera = true;
			m_CameraController = std::make_unique<OrbitalCameraController>(m_InputManager.get(), &m_Camera);
		}
		m_DisableGUI = true;
		m_ToggleFullscreen = true;

		// Load config from command line
		const auto& demo = m_ProfilingDataCollector->GetDemoConfig(0);
		m_Scene = std::make_unique<Scene>(this, 1);

		// Setup camera
		const auto orbitalCamera = static_cast<OrbitalCameraController*>(m_CameraController.get());
		orbitalCamera->SetOrbitPoint(XMLoadFloat3(&demo.CameraConfig.FocalPoint));
		orbitalCamera->SetOrbitRadius(demo.CameraConfig.OrbitRadius);
	}
	else
	{
		// Load default demo
		m_Scene = std::make_unique<Scene>(this, 2);
	}

	// Populate scene
	BaseDemo* demo = BaseDemo::GetDemoFromName("drops");
	const SDFEditList editList = demo->BuildEditList(0);

	m_Factory->BakeSDFSync(m_CurrentPipelineName, m_Geometry.get(), editList);
	// TODO: This is required one first build before passing to Scene::AddGeometry
	// TODO: Is there a better way to handle this?
	m_Geometry->FlipResources();

	m_Scene->AddGeometry(L"Blobs", m_Geometry.get());

	SDFGeometryInstance* instance1 = m_Scene->CreateGeometryInstance(L"Blobs");
	SDFGeometryInstance* instance2 = m_Scene->CreateGeometryInstance(L"Blobs");
	instance2->SetTransform({ 15.0f, 0.0f, 0.0f });

	m_Raytracer->Setup(*m_Scene);

	// Load environment map
	{
		TextureLoader::LoadTextureConfig config = {};
		config.DesiredFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		config.DesiredChannels = 4;
		config.BytesPerChannel = 1;
		config.ResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		auto environmentMap = m_TextureLoader->LoadTextureCubeFromFile("assets/textures/environment.png", &config);
		m_TextureLoader->PerformUploadsImmediatelyAndBlock();
		m_LightManager->ProcessEnvironmentMap(std::move(environmentMap));
	}

	// Set default pass buffer values
	m_PassCB.Flags = RENDER_FLAG_NONE;
	m_PassCB.HeatmapQuantization = 16;
	m_PassCB.HeatmapHueRange = 0.33f;


	auto SetupMaterial = [this](UINT mat, UINT slot, const XMFLOAT3& albedo, float roughness, float metalness, float reflectance)
		{
			const auto pMat = m_MaterialManager->GetMaterial(mat);
			pMat->SetAlbedo(albedo);
			pMat->SetRoughness(roughness);
			pMat->SetMetalness(metalness);
			pMat->SetReflectance(reflectance);
			m_Geometry->SetMaterial(pMat, slot);
		};

	SetupMaterial(0, 0, { 0.0f, 0.0f, 0.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(1, 1, { 1.0f, 1.0f, 1.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(2, 2, { 1.0f, 0.5f, 0.0f }, 0.1f, 0.0f, 0.0f);
	SetupMaterial(3, 3, { 0.0f, 0.9f, 0.9f }, 0.05f, 1.0f, 1.0f);


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

	float deltaTime = m_Timer.GetDeltaTime();

#ifdef ENABLE_INSTRUMENTATION
	// Update profiling before updating the scene, as this may modify the current scene
	m_ProfilingDataCollector->UpdateProfiling(m_Scene.get(), &m_Timer, m_CameraController.get());
#endif

	m_PickingQueryInterface->ReadLastPick();
	m_PickingQueryInterface->SetNextPickLocation({ m_InputManager->GetMouseX(), m_InputManager->GetMouseY() });

	if (m_InputManager->IsKeyPressed(KEY_SPACE))
	{
		m_Paused = !m_Paused;
	}

	bool rebuildOnce = false;
	float timeDirection = 1.0f;
	if (m_Paused)
	{
		if (m_InputManager->IsKeyPressed(KEY_RIGHT))
		{
			rebuildOnce = true;
		}
		else if (m_InputManager->IsKeyPressed(KEY_LEFT))
		{
			rebuildOnce = true;
			timeDirection = -1.0f;
		}
	}

	deltaTime = deltaTime * m_TimeScale * timeDirection * static_cast<float>(!m_Paused || rebuildOnce);

	BaseDemo* demo = BaseDemo::GetDemoFromName("drops");
	m_Factory->BakeSDFSync(m_CurrentPipelineName, m_Geometry.get(), demo->BuildEditList(deltaTime));

	m_Scene->OnUpdate(deltaTime);

	if (m_ShowMainMenuBar && !m_DisableGUI)
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Application Info", nullptr, &m_ShowApplicationInfo);
				ImGui::Separator();
				ImGui::MenuItem("Show Menu", nullptr, &m_ShowMainMenuBar);
	
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
		if (m_ShowApplicationInfo)
			m_ShowApplicationInfo = ImGuiApplicationInfo();
	}

	EndUpdate();
	PIXEndEvent();
}

void D3DApplication::OnRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(1), L"App Render");

	// Update constant buffer
	UpdatePassCB();
	m_MaterialManager->UploadMaterialData();
	m_PickingQueryInterface->UploadPickingParams();

	// Perform all queued uploads
	m_TextureLoader->PerformUploads();

	PROFILE_DIRECT_BEGIN_PASS("Frame");

	// Begin drawing
	m_GraphicsContext->StartDraw(m_PassCB);

	// Tell the scene that render is happening
	// This will update acceleration structures and other things to render the scene
	m_Scene->PreRender();

	// Perform raytracing
	{
		Raytracer::RaytracingParams raytracingParams;
		raytracingParams.PickingInterface = m_PickingQueryInterface.get();
		raytracingParams.MaterialBuffer = m_MaterialManager->GetMaterialBufferAddress();
		raytracingParams.GlobalLightingSRVTable = m_LightManager->GetSRVTable();
		raytracingParams.GlobalLightingSamplerTable = m_LightManager->GetSamplerTable();

		m_Raytracer->DoRaytracing(raytracingParams);
	}
	m_PickingQueryInterface->CopyPickingResult(m_GraphicsContext->GetCommandList());

	m_GraphicsContext->CopyRaytracingOutput(m_Raytracer->GetRaytracingOutput());

	// ImGui Render
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_GraphicsContext->GetCommandList());

	// End draw
	m_GraphicsContext->EndDraw();

	PROFILE_DIRECT_END_PASS();

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

	m_Factory.reset();
	m_Geometry.reset();

	m_Scene.reset();
	m_Raytracer.reset();

	m_LightManager.reset();
	m_MaterialManager.reset();

	m_TextureLoader.reset();

#ifdef ENABLE_INSTRUMENTATION
	GPUProfiler::Destroy();
#endif

	// Release graphics context
	m_GraphicsContext.reset();

	// Cleanup ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void D3DApplication::UpdatePassCB()
{
	// Calculate view matrix
	const XMMATRIX view = m_Camera.GetViewMatrix();
	const XMMATRIX proj = m_GraphicsContext->GetProjectionMatrix();

	const XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	const XMMATRIX invView = XMMatrixInverse(nullptr, view);
	const XMMATRIX invProj = XMMatrixInverse(nullptr, proj);
	const XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	// Update data in main pass constant buffer
	m_PassCB.View = XMMatrixTranspose(view);
	m_PassCB.InvView = XMMatrixTranspose(invView);
	m_PassCB.Proj = XMMatrixTranspose(proj);
	m_PassCB.InvProj = XMMatrixTranspose(invProj);
	m_PassCB.ViewProj = XMMatrixTranspose(viewProj);
	m_PassCB.InvViewProj = XMMatrixTranspose(invViewProj);

	m_PassCB.WorldEyePos = m_Camera.GetPosition();

	m_PassCB.TotalTime = m_Timer.GetTimeSinceReset();
	m_PassCB.DeltaTime = m_Timer.GetDeltaTime();

	// Copy lighting data into the pass buffer
	m_LightManager->CopyLightData(&m_PassCB.Light, 1);
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
	m_CameraController->Update(deltaTime);

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

			m_CameraController->Gui();
		}
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Renderer");
		ImGui::PopStyleColor();

		ImGui::Separator();
		{
			auto RenderFlagOption = [&](const char* name, UINT value)-> bool
				{
					bool flag = m_PassCB.Flags & value;
					if (ImGui::Checkbox(name, &flag))
					{
						if (flag)
							m_PassCB.Flags |= value;
						else
							m_PassCB.Flags &= ~value;
						return true;
					}
					return false;
				};

			ImGui::Text("View Mode");
			RenderFlagOption("Bounding Box", RENDER_FLAG_DISPLAY_BOUNDING_BOX);
			if (m_PassCB.Flags & RENDER_FLAG_DISPLAY_BOUNDING_BOX)
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

			ImGui::Text("Lighting Options");
			RenderFlagOption("Disable IBL", RENDER_FLAG_DISABLE_IBL);
			RenderFlagOption("Disable Skybox", RENDER_FLAG_DISABLE_SKYBOX);
			RenderFlagOption("Disable Shadow", RENDER_FLAG_DISABLE_SHADOW);
			RenderFlagOption("Disable Reflection", RENDER_FLAG_DISABLE_REFLECTION);
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
		ImGui::Text("Lighting");
		m_LightManager->DrawGui();

		ImGui::Separator();
		ImGui::Text("Materials");
		m_MaterialManager->DrawGui();

		ImGui::Separator();

		ImGui::Text("Heatmap");

		int heatmap = static_cast<int>(m_PassCB.HeatmapQuantization);
		if (ImGui::InputInt("Quantization", &heatmap))
		{
			m_PassCB.HeatmapQuantization = static_cast<UINT>(heatmap);
		}
		ImGui::SliderFloat("Hue Range", &m_PassCB.HeatmapHueRange, 0.0f, 1.0f);

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Debug");
		ImGui::PopStyleColor();

		ImGui::Separator();

		{
			const bool pixEnabled = m_GraphicsContext->IsPIXEnabled();
			if (!pixEnabled)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}

			if (ImGui::Button("GPU Capture"))
			{
				D3DDebugTools::PIXGPUCaptureFrame(1);
			}

			if (!pixEnabled)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
		}

		{
			ImGui::Separator();
			const PickingQueryPayload& pick = m_PickingQueryInterface->GetLastPick();
			ImGui::Text("Picking ID: %d", pick.instanceID);
			ImGui::Text("Picking World Pos: %.1f, %.1f, %.1f", pick.hitLocation.x, pick.hitLocation.y, pick.hitLocation.z);
		}
	}
	ImGui::End();
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
