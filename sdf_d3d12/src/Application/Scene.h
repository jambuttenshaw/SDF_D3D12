#pragma once


#include "Renderer/Raytracing/AccelerationStructure.h"
#include "Renderer/Raytracing/GeometryInstance.h"

#include "SDF/SDFObject.h"

class D3DApplication;
class BaseDemo;


class Scene
{
public:
	Scene(D3DApplication* application, UINT maxGeometryInstances);
	virtual ~Scene() = default;

	DISALLOW_COPY(Scene)
	DEFAULT_MOVE(Scene)

	virtual void OnUpdate(float deltaTime) = 0;
	void PreRender();

	void AddGeometry(const std::wstring& name, SDFObject* geometry);
	SDFGeometryInstance* CreateGeometryInstance(const std::wstring& geometryName);

	// Getters
	inline const std::vector<SDFObject*>& GetAllGeometries() const { return m_Geometries; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

	// Gui
	virtual bool DisplayGui() { return false; }
	bool DisplayGeneralGui() const;

private:
	// Handle changes to geometry and update acceleration structure
	void CheckSDFGeometryUpdates() const;
	void UpdateAccelerationStructure();

	// Debug Info
	void DisplaySDFObjectDebugInfo() const;
	void DisplayAccelerationStructureDebugInfo() const;

protected:
	D3DApplication* m_Application = nullptr;

private:
	// A description of all the different types of geometry in the scene
	std::vector<SDFObject*> m_Geometries;
	// A collection of all objects in the scene
	std::vector<SDFGeometryInstance> m_GeometryInstances;

	UINT m_MaxGeometryInstances = 0;
	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;
};
