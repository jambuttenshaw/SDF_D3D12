#pragma once


#include "Renderer/Raytracing/AccelerationStructure.h"

#include "SDF/Factory/SDFFactoryHierarchicalAsync.h"
#include "SDF/SDFObject.h"

class BaseDemo;


class Scene
{
public:
	Scene(const std::string& demoName, float brickSize);
	~Scene();

	DISALLOW_COPY(Scene)
	DISALLOW_MOVE(Scene)

	void OnUpdate(float deltaTime);
	void PreRender();

	// Getters
	inline const std::vector<BottomLevelAccelerationStructureGeometry>& GetAllGeometries() const { return m_SceneGeometry; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

	// Draw ImGui Windows
	void ImGuiSceneMenu();
	bool ImGuiSceneInfo();

private:
	void BuildEditList(float deltaTime, bool async);

	void ReportCounters() const;

	void CheckSDFGeometryUpdates();
	void UpdateAccelerationStructure();

	void DisplaySDFObjectDebugInfo(const wchar_t* name, const SDFObject* object) const;
	void DisplayAccelerationStructureDebugInfo() const;

private:
	BaseDemo* m_CurrentDemo = nullptr;

	// A description of all the different types of geometry in the scene
	std::vector<BottomLevelAccelerationStructureGeometry> m_SceneGeometry;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;

	// Factory
	std::unique_ptr<SDFFactoryHierarchicalAsync> m_Factory;
	std::wstring m_CurrentPipelineName = L"Default";

	// SDF Objects
	std::unique_ptr<SDFObject> m_Object;

	// Demo Scene
	XMMATRIX m_InstanceRotation;
	XMFLOAT3 m_InstanceRotationDelta;

	// GUI controls
	bool m_RotateInstances = true;
	bool m_Rebuild = true;
	bool m_AsyncConstruction = false;
	bool m_EnableEditCulling = true;

	bool m_DisplayDemoGui = true;

	float m_TimeScale = 0.5f;
	bool m_Paused = false;
};
