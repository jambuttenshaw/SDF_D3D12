#pragma once

#include "Renderer/Memory/MemoryAllocator.h"

using Microsoft::WRL::ComPtr;


class ShaderTable;
class Scene;


class Raytracer
{
public:
	Raytracer();
	~Raytracer();

	void Setup(const Scene& scene);
	void DoRaytracing() const;

	void Resize();

	// Getters
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetRaytracingOutputSRV() const { return m_RaytracingOutputDescriptor.GetGPUHandle(1); }

private:
	// Init
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig) const;
	void CreateRootSignatures();
	void CreateRaytracingPipelineStateObject();
	void CreateRaytracingOutputResource();

	// Scene setup

	// Build a shader table for a specific scene
	void BuildShaderTables();

private:
	ComPtr<ID3D12StateObject> m_DXRStateObject;

	// Root signatures
	ComPtr<ID3D12RootSignature> m_RaytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> m_RaytracingLocalRootSignature;

	// Raytracing output
	ComPtr<ID3D12Resource> m_RaytracingOutput;
	DescriptorAllocation m_RaytracingOutputDescriptor;

	// Shader names and Shader tables
	static const wchar_t* c_HitGroupName;
	static const wchar_t* c_RaygenShaderName;
	static const wchar_t* c_IntersectionShaderName;
	static const wchar_t* c_ClosestHitShaderName;
	static const wchar_t* c_MissShaderName;
	std::unique_ptr<ShaderTable> m_MissShaderTable;
	std::unique_ptr<ShaderTable> m_HitGroupShaderTable;
	std::unique_ptr<ShaderTable> m_RayGenShaderTable;

	// The scene to raytrace
	const Scene* m_Scene = nullptr;
};
