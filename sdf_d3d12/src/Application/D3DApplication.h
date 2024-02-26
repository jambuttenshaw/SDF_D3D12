#pragma once

#include "BaseApplication.h"
#include "ProfileConfig.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Framework/Camera.h"
#include "Framework/CameraController.h"
#include "Framework/GameTimer.h"
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

	void InitImGui() const;

	void BeginUpdate();
	void EndUpdate();

	// ImGui Windows
	bool ImGuiApplicationInfo();

private:
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;

	GameTimer m_Timer;
	Camera m_Camera;
	CameraController m_CameraController;

	std::unique_ptr<Scene> m_Scene;
	std::unique_ptr<Raytracer> m_Raytracer;

	// Flags to pass to the renderer
	UINT m_RenderFlags = RENDER_FLAG_DISPLAY_NORMALS;

	UINT m_HeatmapQuantization = 16;
	float m_HeatmapHueRange = 0.33f;

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
	bool m_LoadDefaultProfilingConfig = true;		// If no config was specified in the command line args, default config will be loaded
	ProfileConfig m_ProfileConfig;

	bool m_LoadDefaultGPUProfilerArgs = true;		// If no config was specified in the command line args, default config will be loaded
	GPUProfilerArgs m_GPUProfilerArgs;
};
