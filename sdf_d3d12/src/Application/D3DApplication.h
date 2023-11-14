#pragma once

#include "BaseApplication.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Framework/Camera.h"
#include "Framework/CameraController.h"
#include "Framework/GameTimer.h"

#include "SDF/SDFFactory.h"
#include "SDF/SDFObject.h"

// Forward declarations
class RenderItem;

using Microsoft::WRL::ComPtr;
using namespace DirectX;

enum class DisplayMode
{
	Default,
	DisplayBoundingBox,
	DisplayNormals,
	DisplayHeatmap
};


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

private:
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;

	GameTimer m_Timer;
	Camera m_Camera;
	CameraController m_CameraController;

	DisplayMode m_CurrentDisplayMode = DisplayMode::Default;
	std::map<DisplayMode, std::unique_ptr<D3DComputePipeline>> m_Pipelines;

	std::unique_ptr<SDFFactory> m_SDFFactory;
	std::unique_ptr<SDFObject> m_SDFObject;

	RayMarchPropertiesType m_RayMarchProps {
		0.001f,
		0.02f,
		0.0025f,
		0.02f
	};
};
