#include "pch.h"
#include "CameraController.h"

#include "Input/InputManager.h"
#include "Camera.h"


CameraController::CameraController(InputManager* inputManager, Camera* camera)
	: m_InputManager(inputManager)
	, m_Camera(camera)
{
	// Mouse hidden by default
	m_InputManager->SetMouseHidden(true);
}

void CameraController::Update(float deltaTime) const
{
	if (!(m_InputManager && m_Camera))
	{
		LOG_WARN("Camera controller is not set up!");
		return;
	}

	const float move = m_MoveSpeed * deltaTime;

	// Update camera based on user input
	if (m_InputManager->IsKeyDown(KEY_A))
		m_Camera->Translate({ -move, 0.0f, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_D))
		m_Camera->Translate({ move, 0.0f, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_Q))
		m_Camera->Translate({ 0.0f, -move, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_E))
		m_Camera->Translate({ 0.0f, move, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_S))
		m_Camera->Translate({ 0.0f, 0.0f, -move });
	if (m_InputManager->IsKeyDown(KEY_W))
		m_Camera->Translate({ 0.0f, 0.0f, move });

	// Escape key toggles and un-toggles mouse cursor
	if (m_InputManager->IsKeyPressed(KEY_ESCAPE))
	{
		m_InputManager->SetMouseHidden(!m_InputManager->IsMouseHidden());
	}

	// Don't use mouse input if mouse is not hidden
	if (m_InputManager->IsMouseHidden())
	{
		const float mouseMove = m_RotateSpeed * deltaTime;

		// Rotate from mouse input
		const INT dx = m_InputManager->GetMouseDeltaX();
		const INT dy = m_InputManager->GetMouseDeltaY();
		m_Camera->RotateYaw(mouseMove * static_cast<float>(dx));
		m_Camera->RotatePitch(-mouseMove * static_cast<float>(dy));
	}
}
