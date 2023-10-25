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

	const float move = m_MoveSpeed * deltaTime;
	float strafe = 0.0f;	// how much to move l/r
	float forward = 0.0f;	// how much to move f/b

	// Update camera based on user input
	if (m_InputManager->IsKeyDown(KEY_A))
		strafe -= move;
	if (m_InputManager->IsKeyDown(KEY_D))
		strafe += move;
	if (m_InputManager->IsKeyDown(KEY_S))
		forward -= move;
	if (m_InputManager->IsKeyDown(KEY_W))
		forward += move;

	m_Camera->Translate(strafe * m_Camera->GetRight() * XMVECTOR({ 1.0f, 0.0f, 1.0f }));
	m_Camera->Translate(forward * m_Camera->GetForward() * XMVECTOR({ 1.0f, 0.0f, 1.0f }));

	if (m_InputManager->IsKeyDown(KEY_Q))
		m_Camera->Translate(XMVECTOR{ 0.0f, -move, 0.0f });
	if (m_InputManager->IsKeyDown(KEY_E))
		m_Camera->Translate(XMVECTOR{ 0.0f, move, 0.0f });

	// Escape key toggles and un-toggles mouse cursor
	if (m_InputManager->IsKeyPressed(KEY_ESCAPE))
	{
		m_InputManager->SetMouseHidden(!m_InputManager->IsMouseHidden());
	}

	// Don't use mouse input if mouse is not hidden
	if (m_InputManager->IsMouseHidden() || m_InputManager->IsKeyDown(KEY_RBUTTON))
	{
		const float mouseMove = m_RotateSpeed * deltaTime;

		// Rotate from mouse input
		const INT dx = m_InputManager->GetMouseDeltaX();
		const INT dy = m_InputManager->GetMouseDeltaY();
		m_Camera->RotateYaw(mouseMove * static_cast<float>(dx));
		m_Camera->RotatePitch(-mouseMove * static_cast<float>(dy));
	}
	else
	{
		// Clicking will re-capture the mouse cursor
		if (m_InputManager->IsKeyPressed(KEY_LBUTTON))
		{
			m_InputManager->SetMouseHidden(true);
		}
	}
}
