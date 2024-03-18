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

#include "Renderer/D3DDebugTools.h"



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
	args::Flag fullscreen(windowFlags, "Fullscreen", "Start application in fullscreen", { 'f', "fullscreen" });
	args::Flag noGUI(windowFlags, "No GUI", "Disable GUI", { "no-gui" });

	args::Group graphicsContextFlags(parser, "Graphics Context Flags");
	args::Flag enablePIX(graphicsContextFlags, "Enable GPU Captures", "Enable PIX GPU Captures", { "enable-gpu-capture" });
	args::Flag enableDRED(graphicsContextFlags, "Enable DRED", "Enable Device Removal Extended Data", { "enable-dred" });

	args::Group applicationFlags(parser, "Application Flags");
	args::Flag orbitalCamera(applicationFlags, "Orbital Camera", "Enable an orbital camera", { "orbital-camera" });

#ifdef ENABLE_INSTRUMENTATION
	// These settings won't do anything in a non-instrumented build
	args::Group profilingGroup(parser, "Profiling Options");
	args::ValueFlag<std::string> profileConfig(profilingGroup, "Profile Config", "Path to profiling config file", { "profile-config" });
	args::ValueFlag<std::string> gpuProfileConfig(profilingGroup, "GPU Profiler Config", "Path to GPU profiler config file", { "gpu-profiler-config" });
	args::ValueFlag<std::string> profileOutput(profilingGroup, "Output", "Path to output file", { "profile-output" });

	args::ValueFlag<std::string> outputAvailableMetrics(profilingGroup, "output-available-metrics", "Output file for available GPU metrics", { "output-available-metrics" });
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

#ifdef ENABLE_INSTRUMENTATION
	// These settings aren't relevant otherwise
	if (profileConfig)
	{
		// Parse config
		if (ParseProfileConfigFromJSON(profileConfig.Get(), m_ProfileConfig))
		{
			m_ProfilingMode = true;
			// Force various modes for profiling
			m_UseOrbitalCamera = true;
			m_DisableGUI = true;
			m_ToggleFullscreen = true;

			const auto& demoConfig = m_ProfileConfig.DemoConfigs[0];

			m_CapturesRemaining = m_ProfileConfig.NumCaptures;
			m_DemoIterationsRemaining = demoConfig.IterationCount;
			m_DemoConfigIndex = 0;

			{
				// Build iteration data string
				std::stringstream stream;
				stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << demoConfig.InitialBrickSize << ",";
				m_ConfigData = std::move(stream.str());
			}
		}
	}

	if (gpuProfileConfig)
	{
		// Parse gpu profiler args
		if (ParseGPUProfilerArgsFromJSON(gpuProfileConfig.Get(), m_GPUProfilerArgs))
		{
			// Don't load defaults - args have been supplied
			m_LoadDefaultGPUProfilerArgs = false;
		}
	}

	if (m_ProfilingMode)
	{
		m_ProfileConfig.OutputFile = profileOutput ? profileOutput.Get() : "captures/profile.csv";
		LOG_INFO("Using profiling output path: '{}'", m_ProfileConfig.OutputFile);
	}

	if (outputAvailableMetrics)
	{
		m_OutputAvailableMetrics = true;
		m_AvailableMetricsOutfile = outputAvailableMetrics.Get();
	}
#endif

	return true;
}


void D3DApplication::OnInit()
{
	LOG_INFO("Application startup.");

	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight(), m_GraphicsContextFlags);

	m_TextureLoader = std::make_unique<TextureLoader>();

#ifdef ENABLE_INSTRUMENTATION
	if (m_LoadDefaultGPUProfilerArgs)
	{
		m_GPUProfilerArgs.Queue = GPUProfilerQueue::Direct;
		m_GPUProfilerArgs.Metrics.push_back("gpu__time_duration.sum");
		m_GPUProfilerArgs.Headers.push_back("Duration");
	}

	GPUProfiler::Create(m_GPUProfilerArgs);

	if (m_OutputAvailableMetrics)
	{
		GPUProfiler::GetAvailableMetrics(m_AvailableMetricsOutfile);
	}

	if (m_ProfilingMode)
	{
		// Write headers into outfile
		std::ofstream outFile(m_ProfileConfig.OutputFile);
		if (outFile.good())
		{
			outFile << "Demo Name,Brick Size,BrickCount,EditCount,Range Name";
			for (const auto& header : m_GPUProfilerArgs.Headers)
			{
				outFile << "," << header;
			}
			outFile << std::endl;
		}
		else
		{
			LOG_ERROR("Failed to open outfile '{}'", m_ProfileConfig.OutputFile.c_str());
		}
	}

#endif

	InitImGui();

	// Setup camera

	m_Camera.SetPosition(XMVECTOR{ 0.0f, 3.0f, -8.0f });
	m_Timer.Reset();

	if (m_UseOrbitalCamera)
		m_CameraController = std::make_unique<OrbitalCameraController>(m_InputManager.get(), &m_Camera);
	else
		m_CameraController = std::make_unique<CameraController>(m_InputManager.get(), &m_Camera);

	BaseDemo::CreateAllDemos();
	if (m_ProfilingMode)
	{
		// Load config from command line
		const auto& demo = m_ProfileConfig.DemoConfigs[0];
		m_Scene = std::make_unique<Scene>(demo.DemoName, demo.InitialBrickSize);

		// Setup camera
		const auto orbitalCamera = static_cast<OrbitalCameraController*>(m_CameraController.get());
		orbitalCamera->SetOrbitPoint(XMLoadFloat3(&demo.CameraConfig.FocalPoint));
		orbitalCamera->SetOrbitRadius(demo.CameraConfig.OrbitRadius);
	}
	else
	{
		// Load default demo
		m_Scene = std::make_unique<Scene>(m_DefaultDemo, 0.0625f);
	}

	m_Raytracer = std::make_unique<Raytracer>();
	m_Raytracer->Setup(*m_Scene);

	m_LightManager = std::make_unique<LightManager>();
	m_MaterialManager = std::make_unique<MaterialManager>(1);


	// Load environment map
	{
		TextureLoader::LoadTextureConfig config = {};
		config.DesiredFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		config.DesiredChannels = 4;
		config.BytesPerChannel = 1;
		config.ResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		auto environmentMap = m_TextureLoader->LoadTextureCubeFromFile("assets/textures/environment2.png", &config);
		m_TextureLoader->PerformUploadsImmediatelyAndBlock();
		m_LightManager->ProcessEnvironmentMap(std::move(environmentMap));
	}

	// Populate materials
	MaterialGPUData& mat = m_MaterialManager->GetMaterial(0);
	mat.Albedo = XMFLOAT3(0.0f, 0.27f, 0.89f);
	mat.Roughness = 0.3f;
	mat.Metalness = 1.0f;

	// Set default pass buffer values
	//m_PassCB.Flags = RENDER_FLAG_DISPLAY_NORMALS;
	m_PassCB.HeatmapQuantization = 16;
	m_PassCB.HeatmapHueRange = 0.33f;


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

#ifdef ENABLE_INSTRUMENTATION
	// Update profiling before updating the scene, as this may modify the current scene
	UpdateProfiling();
#endif

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
	UpdatePassCB();
	m_MaterialManager->UploadMaterialData();

	// Perform all queued uploads
	m_TextureLoader->PerformUploads();

	PROFILE_DIRECT_BEGIN_PASS("Frame");

	// Begin drawing
	m_GraphicsContext->StartDraw(m_PassCB);

	// Tell the scene that render is happening
	// This will update acceleration structures and other things to render the scene
	m_Scene->PreRender();

	// Perform raytracing
	m_Raytracer->DoRaytracing(m_MaterialManager->GetMaterialBufferAddress(), m_LightManager->GetSRVTable(), m_LightManager->GetSamplerTable());
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


void D3DApplication::UpdateProfiling()
{
	// This function keeps track of gathering profiling data as specified by the config
	// If no config was given, then no profiling will take place and the application will run in demo mode as normal
	if (!m_ProfilingMode)
		return;

	if (m_Timer.GetTimeSinceReset() < GPUProfiler::GetWarmupTime())
		return;

	if (m_Timer.GetTimeSinceReset() - m_DemoBeginTime < m_DemoWarmupTime)
		return;
	m_Scene->SetPaused(true);

	// Still performing runs to gather data
	if (m_CapturesRemaining > 0)
	{
		// It is safe to access profiler within this function - this will only be called if instrumentation is enabled
		if (GPUProfiler::Get().IsInCollection())
		{
			// If it is in collection, check if it has finished collecting data
			std::vector<std::stringstream> metrics;
			if (GPUProfiler::Get().DecodeData(metrics))
			{
				// data can be gathered from profiler
				m_CapturesRemaining--;

				{
					std::ofstream outFile(m_ProfileConfig.OutputFile, std::ofstream::app);

					if (outFile.good())
					{
						// Build all non gpu profiler data that should also be output
						std::stringstream otherData;
						otherData << m_Scene->GetCurrentBrickCount() << "," << m_Scene->GetDemoEditCount() << ",";

						for (const auto& metric : metrics)
						{
							outFile << m_ConfigData << otherData.str() << metric.str();
						}
					}
					else
					{
						LOG_ERROR("Failed to open outfile '{}'", m_ProfileConfig.OutputFile.c_str());
					}
				}
				
			}
		}
		else
		{
			// Begin a new capture
			PROFILE_CAPTURE_NEXT_FRAME();
		}

	}
	// Check if there are still variations of this demo to perform
	else if (--m_DemoIterationsRemaining > 0)
	{
		// Setup demo for capture
		const DemoConfig& demoConfig = m_ProfileConfig.DemoConfigs[m_DemoConfigIndex];

		// Get current brick size
		float currentBrickSize = 0.0f;
		const UINT iterationsCompleted = demoConfig.IterationCount - m_DemoIterationsRemaining;
		if (demoConfig.IsLinearIncrement)
		{
			currentBrickSize = demoConfig.InitialBrickSize + demoConfig.BrickSizeIncrement * static_cast<float>(iterationsCompleted);
		}
		else
		{
			currentBrickSize = demoConfig.InitialBrickSize * std::pow(demoConfig.BrickSizeMultiplier, static_cast<float>(iterationsCompleted));
		}
		m_Scene->Reset(demoConfig.DemoName, currentBrickSize);

		{
			// Build iteration data string
			std::stringstream stream;
			stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << currentBrickSize << ",";
			m_ConfigData = std::move(stream.str());
		}

		m_CapturesRemaining = m_ProfileConfig.NumCaptures;
	}
	// Current demo config has been completed - progress to the next one
	else if (++m_DemoConfigIndex == m_ProfileConfig.DemoConfigs.size())
	{
		// All demo configs have been captured
		// Process final data
		LOG_INFO("All config profiling completed in {} seconds.", m_Timer.GetTimeSinceReset());

		// Quit application
		Win32Application::ForceQuit();
	}
	else
	{
		// Progress to next demo
		const DemoConfig& demoConfig = m_ProfileConfig.DemoConfigs[m_DemoConfigIndex];

		m_Scene->Reset(demoConfig.DemoName, demoConfig.InitialBrickSize);

		const auto orbitalCamera = static_cast<OrbitalCameraController*>(m_CameraController.get());
		orbitalCamera->SetOrbitPoint(XMLoadFloat3(&demoConfig.CameraConfig.FocalPoint));
		orbitalCamera->SetOrbitRadius(demoConfig.CameraConfig.OrbitRadius);

		{
			// Build iteration data string
			std::stringstream stream;
			stream << std::fixed << std::setprecision(4) << demoConfig.DemoName << "," << demoConfig.InitialBrickSize << ",";
			m_ConfigData = std::move(stream.str());
		}

		m_CapturesRemaining = m_ProfileConfig.NumCaptures;
		m_DemoIterationsRemaining = demoConfig.IterationCount;

		m_DemoBeginTime = m_Timer.GetTimeSinceReset();
		m_Scene->SetPaused(false);
	}
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

			ImGui::Text("Display Options");
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
			RenderFlagOption("Disable IBL", RENDER_FLAG_DISABLE_IBL);
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
