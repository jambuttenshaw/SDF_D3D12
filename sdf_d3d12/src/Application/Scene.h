#pragma once


#include "Renderer/Raytracing/D3DAccelerationStructure.h"


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

private:
	// A description of all the different types of geometry in the scene
	std::vector<BottomLevelAccelerationStructureGeometry> m_SceneGeometry;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;
};