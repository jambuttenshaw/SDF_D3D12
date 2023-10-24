#pragma once

#include "BaseApplication.h"

#include "Camera.h"
#include "GameTimer.h"


// Forward declarations
class D3DGraphicsContext;
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

private:
	std::shared_ptr<D3DGraphicsContext> m_GraphicsContext;

	GameTimer m_Timer;
	Camera m_Camera;

	RenderItem* m_Cube = nullptr;
	float m_CubeRotation = 0.0f;
};
