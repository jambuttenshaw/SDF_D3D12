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

	void UpdatePassCB();

	void InitImGui() const;

	void BeginUpdate();
	void EndUpdate();

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
	std::unique_ptr<PickingQueryInterface> m_PickingQueryInterface;

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

	std::unique_ptr<ProfilingDataCollector> m_ProfilingDataCollector;
};
