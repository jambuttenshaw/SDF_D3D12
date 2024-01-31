#pragma once


#include "Renderer/Raytracing/AccelerationStructure.h"

#include "SDF/SDFFactory.h"
#include "SDF/SDFObject.h"


class Scene
{
public:
	Scene();

	void OnUpdate(float deltaTime);
	void OnRender();

	// Getters
	inline const std::vector<BottomLevelAccelerationStructureGeometry>& GetAllGeometries() const { return m_SceneGeometry; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

	// Draw ImGui Windows
	bool ImGuiSceneInfo();

private:
	void BuildEditList(float deltaTime);

	void UpdateAccelerationStructure();

	void DisplaySDFObjectDebugInfo(const char* name, const SDFObject* object) const;
	void DisplayAccelerationStructureDebugInfo() const;

private:
	// A description of all the different types of geometry in the scene
	std::vector<BottomLevelAccelerationStructureGeometry> m_SceneGeometry;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;

	// SDF Objects
	std::unique_ptr<SDFFactory> m_SDFFactory;

	std::unique_ptr<SDFObject> m_TorusObject;
	std::unique_ptr<SDFObject> m_SphereObject;

	std::unique_ptr<SDFObject> m_Object;

	// Demo Scene
	inline static constexpr UINT s_InstanceGridDims = 3;
	inline static constexpr UINT s_InstanceCount = s_InstanceGridDims * s_InstanceGridDims * s_InstanceGridDims;
	inline static constexpr float s_InstanceSpacing = 8.0f;

	XMMATRIX m_InstanceRotations[s_InstanceCount];
	XMFLOAT3 m_InstanceRotationDeltas[s_InstanceCount];

	XMFLOAT3 m_InstanceTranslation[s_InstanceCount];

	// GUI controls
	bool m_RotateInstances = true;
	bool m_Rebuild = true;

	struct SphereData
	{
		XMFLOAT3 scale;
		XMFLOAT3 speed;
	};
	UINT m_SphereCount = 60;
	std::vector<SphereData> m_SphereData;

	float m_SphereBlend = 0.3f;
};