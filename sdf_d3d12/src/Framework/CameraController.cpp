#include "pch.h"
#include "CameraController.h"

#include "Input/InputManager.h"
#include "Camera.h"


CameraController::CameraController(InputManager* inputManager, Camera* camera)
	: m_InputManager(inputManager)
	, m_Camera(camera)
{
}

void CameraController::Update(float deltaTime) const
{
	if (!(m_InputManager && m_Camera))
	{
		LOG_WARN("Camera controller is not set up!");
		return;
	}

	// Update camera based on user input
	if (m_InputManager->IsKeyDown(KEY_A))
		m_Camera->Translate({ -deltaTime, 0.0f, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_D))
		m_Camera->Translate({ deltaTime, 0.0f, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_Q))
		m_Camera->Translate({ 0.0f, -deltaTime, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_E))
		m_Camera->Translate({ 0.0f, deltaTime, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_S))
		m_Camera->Translate({ 0.0f, 0.0f, -deltaTime});
	if (m_InputManager->IsKeyDown(KEY_W))
		m_Camera->Translate({ 0.0f, 0.0f, deltaTime });

	// Rotate from mouse input
	const INT dx = m_InputManager->GetMouseDeltaX();
	const INT dy = m_InputManager->GetMouseDeltaY();
	m_Camera->RotateYaw(deltaTime * static_cast<float>(dx));
	m_Camera->RotatePitch(-deltaTime * static_cast<float>(dy));
}
