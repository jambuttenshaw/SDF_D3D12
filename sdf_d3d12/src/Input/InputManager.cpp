#include "pch.h"
#include "InputManager.h"


InputManager::InputManager()
{
	for (bool& keyState : m_KeyStates)
	{
		keyState = false;
	}
}

void InputManager::SetKeyDown(KeyCode key)
{
	m_KeyStates[key] = true;
}

void InputManager::SetKeyUp(KeyCode key)
{
	m_KeyStates[key] = false;
}
