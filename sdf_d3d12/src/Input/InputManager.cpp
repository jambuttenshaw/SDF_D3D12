#include "pch.h"
#include "InputManager.h"

#include "Application/BaseApplication.h"
#include "Windows/Win32Application.h"


InputManager::InputManager(BaseApplication* application)
	: m_Application(application)
{
	ASSERT(m_Application, "Input manager requires an application for initialization!");

	for (bool& keyState : m_KeyStates)
	{
		keyState = false;
	}
}

void InputManager::Update(float deltaTime)
{
	// Delta position is calculated differently when the cursor is hidden
	if (m_MouseHidden)
		return;

	// Don't compute delta on first tick
	if (m_FirstTick)
	{
		m_FirstTick = false;
	}
	else
	{
		m_MouseDX = static_cast<INT>(m_MouseX) - static_cast<INT>(m_PrevMouseX);
		m_MouseDY = static_cast<INT>(m_MouseY) - static_cast<INT>(m_PrevMouseY);
	}

	m_PrevMouseX = m_MouseX;
	m_PrevMouseY = m_MouseY;
}

void InputManager::EndFrame()
{
	m_KeysPressed.clear();
	m_KeysReleased.clear();

	m_ScrollDelta = 0;
}


void InputManager::SetKeyDown(KeyCode key)
{
	m_KeyStates[key] = true;
	m_KeysPressed.insert(key);
}

void InputManager::SetKeyUp(KeyCode key)
{
	m_KeyStates[key] = false;
	m_KeysReleased.insert(key);
}

void InputManager::SetMousePosition(UINT x, UINT y)
{
	if (m_MouseHidden)
	{
		const INT cX = static_cast<INT>(m_Application->GetWidth()) / 2;
		const INT cY = static_cast<INT>(m_Application->GetHeight()) / 2;
		Win32Application::MoveCursorToPos(cX, cY);
		 
		m_MouseX = cX;
		m_MouseY = cY;
		m_MouseDX = static_cast<INT>(x) - cX;
		m_MouseDY = static_cast<INT>(y) - cY;
	}
	else
	{
		m_MouseX = x;
		m_MouseY = y;
	}
}

void InputManager::SetScrollDelta(INT delta)
{
	m_ScrollDelta = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA);
}


void InputManager::SetMouseHidden(bool hidden)
{
	m_MouseHidden = hidden;
	ShowCursor(!hidden);

	if (m_MouseHidden)
	{
		const INT cX = static_cast<INT>(m_Application->GetWidth()) / 2;
		const INT cY = static_cast<INT>(m_Application->GetHeight()) / 2;
		Win32Application::MoveCursorToPos(cX, cY);

		m_MouseX = cX;
		m_MouseY = cY;
		m_MouseDX = 0;
		m_MouseDY = 0;
	}
}
