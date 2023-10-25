#pragma once


#include "KeyCodes.h"

class BaseApplication;


class InputManager
{
public:
	InputManager(BaseApplication* application);

	// Called once per frame to update delta values
	void Update(float deltaTime);
	// Called at the end of the frame to clear keys that happened this frame
	void EndFrame();

	// Callbacks to Windows events
	void SetKeyDown(KeyCode key);
	void SetKeyUp(KeyCode key);

	void SetMousePosition(UINT x, UINT y);

	// API for the application to use
	inline bool IsKeyDown(KeyCode key) const { return m_KeyStates[key]; }
	inline bool IsKeyPressed(KeyCode key) const { return m_KeysPressed.find(key) != m_KeysPressed.end(); }
	inline bool IsKeyReleased(KeyCode key) const { return m_KeysReleased.find(key) != m_KeysReleased.end(); }

	// Mouse cursor
	inline UINT GetMouseX() const { return m_MouseX; }
	inline UINT GetMouseY() const { return m_MouseY; }
	inline INT GetMouseDeltaX() const { return m_MouseDX; }
	inline INT GetMouseDeltaY() const { return m_MouseDY; }

	void SetMouseHidden(bool hidden);
	inline bool IsMouseHidden() const { return m_MouseHidden; }

private:
	BaseApplication* m_Application = nullptr;

	// Keyboard
	std::array<bool, KEY_CODE_MAX> m_KeyStates;
	std::set<KeyCode> m_KeysPressed;
	std::set<KeyCode> m_KeysReleased;

	// Mouse
	bool m_MouseHidden = false;

	UINT m_MouseX = 0,
	     m_MouseY = 0;
	UINT m_PrevMouseX = 0,
		 m_PrevMouseY = 0;
	INT m_MouseDX = 0,
		m_MouseDY = 0;

	bool m_FirstTick = true;
};
