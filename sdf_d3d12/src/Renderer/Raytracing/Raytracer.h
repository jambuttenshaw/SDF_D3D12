#pragma once

#include "Core.h"
#include "Renderer/Memory/MemoryAllocator.h"
#include "HlslCompat/RaytracingHlslCompat.h"

using Microsoft::WRL::ComPtr;


class ShaderTable;
class Scene;


class Raytracer
{
public:
	enum GlobalSamplerType
	{
		GlobalSamplerPoint = 0,
		GlobalSamplerLinear = 1,
		GlobalSamplerCount
	};

	struct RaytracingParams
	{
		class Picker* PickingInterface;

		D3D12_GPU_VIRTUAL_ADDRESS MaterialBuffer;

		D3D12_GPU_DESCRIPTOR_HANDLE GlobalLightingSRVTable;
		D3D12_GPU_DESCRIPTOR_HANDLE GlobalLightingSamplerTable;
	};

public:
	Raytracer();
	~Raytracer();

	void Setup(const Scene& scene);
	void DoRaytracing(const RaytracingParams& params) const;

	void Resize();

	inline GlobalSamplerType GetCurrentSampler() const { return m_CurrentSampler; }
	inline void SetCurrentSampler(GlobalSamplerType sampler) { m_CurrentSampler = sampler; }

	inline ID3D12Resource* GetRaytracingOutput() const { return m_RaytracingOutput.Get(); }

private:
	// Init
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const;
	void CreateRootSignatures();
	void CreateRaytracingPipelineStateObject();
	void CreateRaytracingOutputResource();
	void CreateSamplers();

	// Scene setup

	// Build a shader table for a specific scene
	void BuildShaderTables();
	// Updates the entries in the hit group shader table
	// This is used when local root arguments change
	void UpdateHitGroupShaderTable() const;
	 
private:
	ComPtr<ID3D12StateObject> m_DXRStateObject;

	// Root signatures
	ComPtr<ID3D12RootSignature> m_RaytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> m_RaytracingLocalRootSignature;

	// Raytracing output
	ComPtr<ID3D12Resource> m_RaytracingOutput;
	DescriptorAllocation m_RaytracingOutputDescriptor;

	// Shader names and Shader tables
	static const wchar_t* c_HitGroupName[RayType_Count];
	static const wchar_t* c_RaygenShaderName;
	static const wchar_t* c_IntersectionShaderName;
	static const wchar_t* c_ClosestHitShaderName;
	static const wchar_t* c_MissShaderName[RayType_Count];
	std::unique_ptr<ShaderTable> m_MissShaderTable;
	std::unique_ptr<ShaderTable> m_HitGroupShaderTable;
	std::unique_ptr<ShaderTable> m_RayGenShaderTable;

	// Samplers
	DescriptorAllocation m_Samplers;	// Contains 2 samplers:
										// [0] -> Point sampler
										// [1] -> Linear sampler
	GlobalSamplerType m_CurrentSampler = GlobalSamplerLinear;

	// The scene to raytrace
	const Scene* m_Scene = nullptr;
};
