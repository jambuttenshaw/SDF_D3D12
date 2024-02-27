#pragma once


#include "CameraController.h"

using namespace DirectX;


class OrbitalCameraController : public CameraController
{
public:
	OrbitalCameraController() = default;
	OrbitalCameraController(InputManager* inputManager, Camera* camera);
	virtual ~OrbitalCameraController() override = default;

	DEFAULT_COPY(OrbitalCameraController)
	DEFAULT_MOVE(OrbitalCameraController)

	virtual void Update(float deltaTime) override;
	virtual void Gui() override;

protected:
	// Manipulable parameters
	XMVECTOR m_OrbitPoint{ 0.0f, 0.0f, 0.0f };
	float m_OrbitRadius = 12.0f;
	float m_AngularSpeed = 0.5f; // rad/s

	float m_Angle = 0.0f;
};
