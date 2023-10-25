#pragma once

#include "Input/InputManager.h"


class BaseApplication
{
public:
	BaseApplication(UINT width, UINT height, const std::wstring& name);
	virtual ~BaseApplication() = default;

	virtual void OnInit() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnDestroy() = 0;

	// Input callbacks
	void OnKeyUp(UINT8 key) const;
	void OnKeyDown(UINT8 key) const;

	void OnMouseButtonDown(UINT8 mouseButton) const;
	void OnMouseButtonUp(UINT8 mouseButton) const;
	void OnMouseMove(UINT X, UINT Y) const;

	// Resize callbacks
	void BeginResize();
	void Resize(UINT width, UINT height);
	void EndResize();

	// Getters
	inline UINT GetWidth() const			{ return m_Width; }
	inline UINT GetHeight() const			{ return m_Height; }
	inline const WCHAR* GetTitle() const	{ return m_Title.c_str(); }

	virtual void ParseCommandLineArgs(WCHAR* argv[], int argc) {}

protected:
	// Callback event to be implemented by client applications
	virtual void OnResized() {}

	void SetCustomWindowText(LPCWSTR text) const;

protected:
	// viewport dimensions
	UINT m_Width = 0,
		m_Height = 0;
	 
	float m_AspectRatio = 1.0f;

	std::unique_ptr<InputManager> m_InputManager;

private:
	// Root assets path
	std::wstring m_AssetsPath;

	// Window title
	std::wstring m_Title;

	// For detecting if a back buffer resize is actually required
	UINT m_PreviousWidth = 0,
		m_PreviousHeight = 0;
};
