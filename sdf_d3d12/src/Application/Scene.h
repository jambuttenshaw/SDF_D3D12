#pragma once


#include "Renderer/Raytracing/AccelerationStructure.h"

#include "SDF/Factory/SDFFactoryHierarchicalAsync.h"
#include "SDF/SDFObject.h"


class Scene
{
public:
	Scene();
	~Scene();

	DISALLOW_COPY(Scene)
	DISALLOW_MOVE(Scene)

	void OnUpdate(float deltaTime);
	void PreRender();
	void PostRender();

	// Getters
	inline const std::vector<BottomLevelAccelerationStructureGeometry>& GetAllGeometries() const { return m_SceneGeometry; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

	// Draw ImGui Windows
	bool ImGuiSceneInfo();

private:
	void BuildEditList(float deltaTime, bool async);

	void CheckSDFGeometryUpdates();
	void UpdateAccelerationStructure();

	void DisplaySDFObjectDebugInfo(const wchar_t* name, const SDFObject* object) const;
	void DisplayAccelerationStructureDebugInfo() const;

private:
	// A description of all the different types of geometry in the scene
	std::vector<BottomLevelAccelerationStructureGeometry> m_SceneGeometry;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;

	// SDF Objects
	std::unique_ptr<SDFFactoryHierarchicalAsync> m_SDFFactoryHierarchicalAsync;

	std::unique_ptr<SDFObject> m_BlobObject;
	std::unique_ptr<SDFObject> m_FrameObject;

	// Demo Scene
	inline static constexpr UINT s_InstanceCount = 2;

	XMMATRIX m_InstanceRotations[s_InstanceCount];
	XMFLOAT3 m_InstanceRotationDeltas[s_InstanceCount];

	// GUI controls
	bool m_RotateInstances = true;
	bool m_Rebuild = false;
	bool m_AsyncConstruction = false;

	float m_TimeScale = 0.5f;

	struct SphereData
	{
		XMFLOAT3 scale;
		XMFLOAT3 speed;
	};
	UINT m_SphereCount = 100;
	std::vector<SphereData> m_SphereData;
	float m_SphereBlend = 0.4f;
};
