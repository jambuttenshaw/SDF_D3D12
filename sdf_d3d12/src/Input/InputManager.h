#pragma once


#include "KeyCodes.h"

class InputManager
{
public:
	InputManager();

	void SetKeyDown(KeyCode key);
	void SetKeyUp(KeyCode key);

	inline bool IsKeyDown(KeyCode key) const { return m_KeyStates[key]; }

private:
	inline static constexpr KeyCode s_KeyCount{ KEY_CODE_MAX };
	std::array<bool, KEY_CODE_MAX> m_KeyStates;
};
