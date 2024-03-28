#include "pch.h"
#include "Material.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include "Renderer/D3DGraphicsContext.h"


Material::Material(UINT materialID)
	: m_MaterialID(materialID)
	, m_NumFramesDirty(D3DGraphicsContext::GetBackBufferCount())
{
	m_Name = std::to_string(m_MaterialID);

	// Defaults
	m_Data.Albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
	m_Data.Roughness = 0.4f;
	m_Data.Metalness = 0.0f;
	m_Data.Reflectance = 0.0f;
}

void Material::SetDirty()
{
	m_NumFramesDirty = D3DGraphicsContext::GetBackBufferCount();
}

void Material::DrawGui()
{
	bool dirty = false;

	// Name editing
	ImGui::InputText("Name", &m_Name);

	dirty |= ImGui::ColorEdit3("Albedo", &m_Data.Albedo.x);
	dirty |= ImGui::SliderFloat("Roughness", &m_Data.Roughness, 0.05f, 1.0f);
	dirty |= ImGui::SliderFloat("Metalness", &m_Data.Metalness, 0.0f, 1.0f);
	dirty |= ImGui::SliderFloat("Reflectance", &m_Data.Reflectance, 0.0f, 1.0f);

	if (dirty) 
		SetDirty();
}


MaterialManager::MaterialManager(size_t capacity)
	: m_MaterialBuffers(D3DGraphicsContext::GetBackBufferCount())
{
	m_Materials.reserve(capacity);
	for (UINT id = 0; id < static_cast<UINT>(capacity); id++)
	{
		m_Materials.emplace_back(Material{ id });
	}

	// const UINT alignment = Align(static_cast<UINT>(sizeof(MaterialGPUData)), 16);
	for (auto& buffer : m_MaterialBuffers)
	{
		buffer.Allocate(g_D3DGraphicsContext->GetDevice(), static_cast<UINT>(capacity), 0, L"Material Buffer");
	}
}


void MaterialManager::UploadMaterialData()
{
	const auto& matBuffer = m_MaterialBuffers.at(g_D3DGraphicsContext->GetCurrentBackBuffer());

	for (auto& mat : m_Materials)
	{
		if (mat.m_NumFramesDirty > 0)
		{
			matBuffer.CopyElement(mat.m_MaterialID, mat.m_Data);
			mat.m_NumFramesDirty--;
		}
	}
}

D3D12_GPU_VIRTUAL_ADDRESS MaterialManager::GetMaterialBufferAddress() const
{
	return m_MaterialBuffers.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).GetAddressOfElement(0);
}


void MaterialManager::DrawGui()
{
	static int selectedMat = 0;
	DrawMaterialComboGui("Material", selectedMat);
	m_Materials.at(selectedMat).DrawGui();
}


bool MaterialManager::DrawMaterialComboGui(const char* label, int& material)
{
	auto matNameGetter = [](void* materials, int index) -> const char*
		{
			const Material* m = static_cast<Material*>(materials) + index;
			return m->GetName();
		};
	return ImGui::Combo(label, &material, matNameGetter, m_Materials.data(), static_cast<int>(m_Materials.size()));
}
