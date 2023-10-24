#pragma once


#include "KeyCodes.h"

class InputManager
{
public:
	InputManager();

	// Called once per frame to update delta values
	void Update(float deltaTime);

	// Callbacks to Windows events
	void SetKeyDown(KeyCode key);
	void SetKeyUp(KeyCode key);

	void SetMousePosition(UINT x, UINT y);

	// API for the application to use
	inline bool IsKeyDown(KeyCode key) const { return m_KeyStates[key]; }

	inline UINT GetMouseX() const { return m_MouseX; }
	inline UINT GetMouseY() const { return m_MouseY; }
	inline INT GetMouseDeltaX() const { return m_MouseDX; }
	inline INT GetMouseDeltaY() const { return m_MouseDY; }

private:
	// Keyboard
	std::array<bool, KEY_CODE_MAX> m_KeyStates;

	// Mouse
	UINT m_MouseX = 0,
	     m_MouseY = 0;
	UINT m_PrevMouseX = 0,
		 m_PrevMouseY = 0;
	INT m_MouseDX = 0,
		m_MouseDY = 0;

	bool m_FirstTick = true;
};
