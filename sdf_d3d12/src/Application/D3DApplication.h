#pragma once

#include "BaseApplication.h"

#include "Input/InputManager.h"
#include "Renderer/D3DGraphicsContext.h"
#include "Framework/Camera.h"
#include "Framework/CameraController.h"
#include "Framework/GameTimer.h"


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

	// Input callbacks
	virtual void OnKeyDown(UINT8 key) override;
	virtual void OnKeyUp(UINT8 key) override;
	virtual void OnMouseMove(UINT x, UINT y) override;

private:

	void InitImGui() const;

private:
	std::unique_ptr<InputManager> m_InputManager;
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;

	GameTimer m_Timer;
	Camera m_Camera;
	CameraController m_CameraController;

	RenderItem* m_Cube = nullptr;
	float m_CubeRotation = 0.0f;
};
