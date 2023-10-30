#include "pch.h"
#include "BaseApplication.h"

#include "imgui.h"
#include "Windows/Win32Application.h"

BaseApplication::BaseApplication(UINT width, UINT height, const std::wstring& name)
	: m_Width(width)
	, m_Height(height)
	, m_Title(name)
{
	// init asset file path

	m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);

	// Create input manager
	m_InputManager = std::make_unique<InputManager>(this);
}

void BaseApplication::SetCustomWindowText(LPCWSTR text) const
{
	const std::wstring windowText = m_Title + L": " + text;
	SetWindowText(Win32Application::GetHwnd(), windowText.c_str());
}

void BaseApplication::OnKeyDown(UINT8 key) const
{
	if (!ImGui::GetIO().WantCaptureKeyboard)
		m_InputManager->SetKeyDown(KeyCode{ key });
}

void BaseApplication::OnKeyUp(UINT8 key) const
{
	if (!ImGui::GetIO().WantCaptureKeyboard)
		m_InputManager->SetKeyUp(KeyCode{ key });
}

void BaseApplication::OnMouseButtonDown(UINT8 mouseButton) const
{
	if (!ImGui::GetIO().WantCaptureMouse)
		m_InputManager->SetKeyDown(KeyCode{ mouseButton });
}

void BaseApplication::OnMouseButtonUp(UINT8 mouseButton) const
{
	if (!ImGui::GetIO().WantCaptureMouse)
		m_InputManager->SetKeyUp(KeyCode{ mouseButton });
}

void BaseApplication::OnMouseMove(UINT x, UINT y) const
{
	m_InputManager->SetMousePosition(x, y);
}

void BaseApplication::BeginResize()
{
	m_PreviousWidth = m_Width;
	m_PreviousHeight = m_Height;
}

void BaseApplication::Resize(UINT width, UINT height)
{
	m_Width = width;
	m_Height = height;
	m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void BaseApplication::EndResize()
{
	if (m_Width != m_PreviousWidth || m_Height != m_PreviousHeight)
	{
		OnResized();
	}
}

