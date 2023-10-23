#pragma once

#include "BaseApplication.h"

#include "Renderer/RenderItem.h"


// Forward declarations
class D3DGraphicsContext;

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

private:

	void InitImGui() const;

private:
	std::shared_ptr<D3DGraphicsContext> m_GraphicsContext;
};
