#include "pch.h"
#include "InputManager.h"


InputManager::InputManager()
{
	for (bool& keyState : m_KeyStates)
	{
		keyState = false;
	}
}

void InputManager::Update(float deltaTime)
{
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


void InputManager::SetKeyDown(KeyCode key)
{
	m_KeyStates[key] = true;
}

void InputManager::SetKeyUp(KeyCode key)
{
	m_KeyStates[key] = false;
}

void InputManager::SetMousePosition(UINT x, UINT y)
{
	m_MouseX = x;
	m_MouseY = y;
}
