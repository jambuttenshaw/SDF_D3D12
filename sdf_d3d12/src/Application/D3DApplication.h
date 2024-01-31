#pragma once

#include "BaseApplication.h"
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

	virtual void OnInit() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnDestroy() override;

	virtual void OnResized() override;
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

	// GUI
	bool m_ShowImGuiDemo = false;
	bool m_ShowApplicationInfo = true;
	bool m_ShowSceneInfo = true;
};
