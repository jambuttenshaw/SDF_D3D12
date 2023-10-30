#pragma once

class InputManager;
class Camera;


class CameraController
{
public:
	CameraController() = default;
	CameraController(InputManager* inputManager, Camera* camera);

	void Update(float deltaTime);
	void Gui();

private:
	InputManager* m_InputManager = nullptr;
	Camera* m_Camera = nullptr;

	// Controller properties
	float m_MoveSpeed = 3.0f;
	float m_RotateSpeed = 0.15f;

	float m_ScrollSpeed = 1.5f;

	static constexpr float s_MinMoveSpeed = 0.1f;
	static constexpr float s_MaxMoveSpeed = 20.0f;
};
