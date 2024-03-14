#pragma once

#include "BaseApplication.h"
#include "ProfileConfig.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Framework/Camera/Camera.h"
#include "Framework/Camera/OrbitalCameraController.h"
#include "Framework/GameTimer.h"
#include "Renderer/Buffer/TextureLoader.h"
#include "Renderer/Lighting/Light.h"
#include "Renderer/Lighting/Material.h"
#include "Renderer/Raytracing/Raytracer.h"


// Forward declarations
class RenderItem;

using Microsoft::WRL::ComPtr;
using namespace DirectX;


class D3DApplication : public BaseApplication
{
public:
	D3DApplication(UINT width, UINT height, const std::wstring& name);
	virtual ~D3DApplication() override = default;

	virtual bool ParseCommandLineArgs(LPWSTR argv[], int argc) override;

	virtual void OnInit() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnDestroy() override;

	virtual void OnResized() override;

	virtual bool GetTearingSupport() const override;
	virtual IDXGISwapChain* GetSwapChain() const override;
private:

	void UpdatePassCB();

	void InitImGui() const;

	void BeginUpdate();
	void EndUpdate();

	void UpdateProfiling();

	// ImGui Windows
	bool ImGuiApplicationInfo();

private:
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;
	std::unique_ptr<TextureLoader> m_TextureLoader;

	GameTimer m_Timer;
	Camera m_Camera;

	bool m_UseOrbitalCamera = false;
	std::unique_ptr<CameraController> m_CameraController;

	std::unique_ptr<Scene> m_Scene;
	std::unique_ptr<Raytracer> m_Raytracer;

	std::unique_ptr<LightManager> m_LightManager;
	std::unique_ptr<MaterialManager> m_MaterialManager;

	PassConstantBuffer m_PassCB;


	std::string m_DefaultDemo = "drops";

	// GUI
	bool m_DisableGUI = false;
	bool m_ShowMainMenuBar = true;
	bool m_ShowImGuiDemo = false;
	bool m_ShowApplicationInfo = true;
	bool m_ShowSceneInfo = true;

	// Should the application toggle fullscreen on the next update
	bool m_ToggleFullscreen = false;
	D3DGraphicsContextFlags m_GraphicsContextFlags;

	// Profiling configuration
	bool m_ProfilingMode = false;		
	ProfileConfig m_ProfileConfig;

	UINT m_CapturesRemaining = 0;
	bool m_BegunCapture = false;

	UINT m_DemoIterationsRemaining = 0;
	UINT m_DemoConfigIndex = 0;

	float m_DemoBeginTime = 0.0f;
	float m_DemoWarmupTime = 5.0f;

	std::string m_ConfigData;

	bool m_LoadDefaultGPUProfilerArgs = true;		// If no config was specified in the command line args, default config will be loaded
	GPUProfilerArgs m_GPUProfilerArgs;

	bool m_OutputAvailableMetrics = false;
	std::string m_AvailableMetricsOutfile;
};
