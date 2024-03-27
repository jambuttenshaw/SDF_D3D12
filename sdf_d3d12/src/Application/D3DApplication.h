#pragma once

#include "BaseApplication.h"
#include "ProfileConfig.h"
#include "ProfilingDataCollector.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/TextureLoader.h"
#include "Renderer/Lighting/Light.h"
#include "Renderer/Lighting/Material.h"
#include "Renderer/Raytracing/Raytracer.h"

#include "Framework/PickingQueryInterface.h"
#include "Framework/GameTimer.h"
#include "Framework/Camera/Camera.h"
#include "Framework/Camera/OrbitalCameraController.h"
#include "SDF/Factory/SDFFactoryHierarchicalAsync.h"


// Forward declarations
class RenderItem;

using Microsoft::WRL::ComPtr;
using namespace DirectX;


class D3DApplication : public BaseApplication
{
public:
	D3DApplication(UINT width, UINT height, const std::wstring& name);
	virtual ~D3DApplication() override = default;

	DISALLOW_COPY(D3DApplication)
	DISALLOW_MOVE(D3DApplication)

	virtual bool ParseCommandLineArgs(LPWSTR argv[], int argc) override;

	virtual void OnInit() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnDestroy() override;

	virtual void OnResized() override;

	virtual bool GetTearingSupport() const override;
	virtual IDXGISwapChain* GetSwapChain() const override;

	inline InputManager* GetInputManager() const { return m_InputManager.get(); }
	inline LightManager* GetLightManager() const { return m_LightManager.get(); }
	inline MaterialManager* GetMaterialManager() const { return m_MaterialManager.get(); }

private:

	inline bool GetPaused() const { return m_Paused; }
	inline void SetPaused(bool paused) { m_Paused = paused; }

	void UpdatePassCB();

	void InitImGui() const;

	void BeginUpdate();
	void EndUpdate();

	// ImGui Windows
	bool ImGuiApplicationInfo();

private:
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;
	std::unique_ptr<TextureLoader> m_TextureLoader;

	PassConstantBuffer m_PassCB;

	GameTimer m_Timer;
	Camera m_Camera;

	bool m_UseOrbitalCamera = false;
	std::unique_ptr<CameraController> m_CameraController;

	std::unique_ptr<Scene> m_Scene;

	std::unique_ptr<Raytracer> m_Raytracer;
	std::unique_ptr<PickingQueryInterface> m_PickingQueryInterface;

	std::unique_ptr<LightManager> m_LightManager;
	std::unique_ptr<MaterialManager> m_MaterialManager;

	// Factory
	std::unique_ptr<SDFFactoryHierarchicalAsync> m_Factory;
	std::wstring m_CurrentPipelineName = L"Default";

	std::unique_ptr<SDFObject> m_Geometry;

	// GUI
	bool m_DisableGUI = false;
	bool m_ShowMainMenuBar = true;
	bool m_ShowImGuiDemo = false;
	bool m_ShowApplicationInfo = true;
	bool m_ShowSceneInfo = true;

	bool m_Rebuild = true;
	bool m_AsyncConstruction = false;
	bool m_EnableEditCulling = true;

	bool m_DisplayDemoGui = true;

	float m_TimeScale = 0.5f;
	bool m_Paused = true;

	// Should the application toggle fullscreen on the next update
	bool m_ToggleFullscreen = false;
	D3DGraphicsContextFlags m_GraphicsContextFlags;

	std::unique_ptr<ProfilingDataCollector> m_ProfilingDataCollector;
};
