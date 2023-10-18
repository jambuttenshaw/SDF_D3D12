#pragma once


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
	virtual void OnKeyUp(UINT8 key)		{}
	virtual void OnKeyDown(UINT8 key)	{}

	// Resize callbacks
	virtual void Resize(UINT width, UINT height) { m_Width = width; m_Height = height; }
	virtual void OnResized() {}

	// Getters
	inline UINT GetWidth() const			{ return m_Width; }
	inline UINT GetHeight() const			{ return m_Height; }
	inline const WCHAR* GetTitle() const	{ return m_Title.c_str(); }

	virtual void ParseCommandLineArgs(WCHAR* argv[], int argc) {}

protected:
	void SetCustomWindowText(LPCWSTR text) const;

protected:
	// viewport dimensions
	UINT m_Width = 0,
		m_Height = 0;
	 
	float m_AspectRatio = 1.0f;

private:
	// Root assets path
	std::wstring m_AssetsPath;

	// Window title
	std::wstring m_Title;
};
