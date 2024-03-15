#include "pch.h"
#include "Material.h"

#include "imgui.h"
#include "Renderer/D3DGraphicsContext.h"


MaterialManager::MaterialManager(size_t capacity)
	: m_MaterialStaging(capacity)
	, m_MaterialBuffers(D3DGraphicsContext::GetBackBufferCount())
{
	const UINT alignment = Align(static_cast<UINT>(sizeof(MaterialGPUData)), 16);
	for (auto& buffer : m_MaterialBuffers)
	{
		buffer.Allocate(g_D3DGraphicsContext->GetDevice(), static_cast<UINT>(capacity), alignment, L"Material Buffer");
	}
}


void MaterialManager::UploadMaterialData() const
{
	m_MaterialBuffers.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).CopyElements(0, static_cast<UINT>(m_MaterialStaging.size()), m_MaterialStaging.data());
}

D3D12_GPU_VIRTUAL_ADDRESS MaterialManager::GetMaterialBufferAddress() const
{
	return m_MaterialBuffers.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).GetAddressOfElement(0);
}


void MaterialManager::DrawGui()
{
	for (auto& material : m_MaterialStaging)
	{
		ImGui::ColorEdit3("Albedo", &material.Albedo.x);
		ImGui::SliderFloat("Roughness", &material.Roughness, 0.05f, 1.0f);
		ImGui::SliderFloat("Metalness", &material.Metalness, 0.0f, 1.0f);
	}
}
