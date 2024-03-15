#pragma once

#include "Core.h"
#include "HlslCompat/RaytracingHlslCompat.h"
#include "Renderer/D3DPipeline.h"

#include "Renderer/Buffer/Texture.h"

using namespace DirectX;


namespace GlobalLightingPipeline
{
	enum Value
	{
		IrradianceMap = 0,
		BRDFIntegration,
		PreFilteredEnvironmentMap,
		Count
	};
}

class LightManager
{
	// This only contains SRVs so that all global lighting descriptors can be contained within one table
	enum GlobalLightingSRV
	{
		EnvironmentMapSRV = 0,
		IrradianceMapSRV,
		BRDFIntegrationMapSRV,
		PreFilteredEnvironmentMapSRV,
		SRVCount
	};
	enum Samplers
	{
		EnvironmentMapSampler = 0,
		BRDFIntegrationMapSampler,
		SamplerCount
	};

public:
	LightManager();
	~LightManager();

	DISALLOW_COPY(LightManager);
	DEFAULT_MOVE(LightManager);

	void CopyLightData(LightGPUData* dest, size_t maxLights) const;

	void ProcessEnvironmentMap(std::unique_ptr<Texture>&& map);

	void DrawGui();

public:
	// Get resources
	inline Texture* GetEnvironmentMap() const { return m_EnvironmentMap.get(); }

	// Get descriptors
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSRVTable() const { return m_GlobalLightingSRVs.GetGPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerTable() const { return m_SamplerDescriptors.GetGPUHandle(); }

private:
	void CreatePipelines();
	void CreateResources();

private:
	inline static constexpr size_t s_MaxLights = 1;

	std::array<LightGPUData, s_MaxLights> m_Lights;

	// Environmental lighting resources
	std::unique_ptr<Texture> m_EnvironmentMap;


	UINT m_IrradianceMapResolution = 32;
	UINT m_BRDFIntegrationMapResolution = 512;
	UINT m_PEMResolution = 128;
	UINT m_PEMRoughnessBins = 5;

	// Pre-processed image-based lighting resources
	std::unique_ptr<Texture> m_IrradianceMap;
	std::unique_ptr<Texture> m_BRDFIntegrationMap;
	std::unique_ptr<Texture> m_PreFilteredEnvironmentMap;

	// All descriptors for global lighting
	DescriptorAllocation m_GlobalLightingSRVs;
	DescriptorAllocation m_SamplerDescriptors;

	// UAVs for each face of the irradiance map
	DescriptorAllocation m_IrradianceMapFaceUAVs;
	DescriptorAllocation m_BRDFIntegrationMapUAV;
	// UAVs for each face and each mip of the PEM
	DescriptorAllocation m_PEMFaceUAVs;
	
	// API resources to process the environment
	// As this is not an operation that occurs often it will just block the render queue to process the environment map
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
	std::array<std::unique_ptr<D3DComputePipeline>, GlobalLightingPipeline::Count> m_Pipelines;

	UINT64 m_PreviousWorkFence = 0;
};
