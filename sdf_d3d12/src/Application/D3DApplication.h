#pragma once

#include "BaseApplication.h"

// Forward declarations
class D3DGraphicsContext;
class D3DConstantBuffer;

using Microsoft::WRL::ComPtr;
using namespace DirectX;


class D3DApplication : public BaseApplication
{
	struct ConstantBufferType
	{
		XMFLOAT4 colorMultiplier{ 1.0f, 1.0f, 1.0f, 1.0f };
		float padding[60];
	};
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

	ConstantBufferType m_ConstantBufferData = {};
	std::shared_ptr<D3DConstantBuffer> m_ConstantBuffer;
};
