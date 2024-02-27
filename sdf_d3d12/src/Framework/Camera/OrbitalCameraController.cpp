#include "pch.h"
#include "OrbitalCameraController.h"

#include "Camera.h"
#include "imgui.h"


OrbitalCameraController::OrbitalCameraController(InputManager* inputManager, Camera* camera)
	: CameraController(inputManager, camera)
{
}


void OrbitalCameraController::Update(float deltaTime)
{
	m_Angle += m_AngularSpeed * deltaTime;

	// Calculate camera position
	const XMVECTOR position = m_OrbitRadius * XMVECTOR{ sinf(m_Angle), 0.0f, cosf(m_Angle) };
	m_Camera->SetPosition(m_OrbitPoint + position);

	XMFLOAT3 forward;
	XMStoreFloat3(&forward, XMVector3Normalize(m_OrbitPoint - position));

	const float pitch = -asinf(forward.y);
	float yaw = asinf(forward.x / cosf(pitch));
	if (forward.z < 0)
		yaw = XM_PI - yaw;

	m_Camera->SetPitch(pitch);
	m_Camera->SetYaw(yaw);
}


void OrbitalCameraController::Gui()
{
	ImGui::Text("Orbit Controls");

	XMFLOAT3 pos;
	XMStoreFloat3(&pos, m_OrbitPoint);
	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
	{
		m_OrbitPoint = XMLoadFloat3(&pos);
	}

	ImGui::DragFloat("Radius", &m_OrbitRadius, 0.01f);

	ImGui::DragFloat("Speed", &m_AngularSpeed, 0.01f);
}
