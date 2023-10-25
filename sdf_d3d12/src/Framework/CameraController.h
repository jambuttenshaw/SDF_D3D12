#pragma once

class InputManager;
class Camera;


class CameraController
{
public:
	CameraController() = default;
	CameraController(InputManager* inputManager, Camera* camera);

	void Update(float deltaTime) const;

private:
	InputManager* m_InputManager = nullptr;
	Camera* m_Camera = nullptr;

	// Controller properties
	float m_MoveSpeed = 3.0f;
	float m_RotateSpeed = 0.2f;
};
