#pragma once


#include "Renderer/Raytracing/AccelerationStructure.h"

#include "SDF/SDFFactory.h"
#include "SDF/SDFObject.h"
#include "SDF/SDFEditList.h"


class Scene
{
public:
	Scene();

	void OnUpdate(float deltaTime);
	void OnRender();

	// Getters
	inline const std::vector<BottomLevelAccelerationStructureGeometry>& GetAllGeometries() const { return m_SceneGeometry; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

private:
	void UpdateAccelerationStructure();

	void DebugInfo(const char* name, const SDFObject* object) const;

private:
	// A description of all the different types of geometry in the scene
	std::vector<BottomLevelAccelerationStructureGeometry> m_SceneGeometry;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;

	// SDF Objects
	std::unique_ptr<SDFFactory> m_SDFFactory;

	std::unique_ptr<SDFObject> m_TorusObject;
	std::unique_ptr<SDFObject> m_SphereObject;

	// Demo Scene
	inline static constexpr UINT s_InstanceGridDims = 2;
	inline static constexpr UINT s_InstanceCount = s_InstanceGridDims * s_InstanceGridDims * s_InstanceGridDims;
	inline static constexpr float s_InstanceSpacing = 4.0f;

	XMMATRIX m_InstanceRotations[s_InstanceCount];
	XMFLOAT3 m_InstanceRotationDeltas[s_InstanceCount];

	XMFLOAT3 m_InstanceTranslation[s_InstanceCount];

	// GUI controls
	bool m_RotateInstances = true;
};