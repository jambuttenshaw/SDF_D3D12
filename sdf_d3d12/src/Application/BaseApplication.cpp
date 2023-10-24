#include "pch.h"
#include "BaseApplication.h"

#include "Windows/Win32Application.h"

BaseApplication::BaseApplication(UINT width, UINT height, const std::wstring& name)
	: m_Width(width)
	, m_Height(height)
	, m_Title(name)
{
	// init asset file path

	m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void BaseApplication::SetCustomWindowText(LPCWSTR text) const
{
	const std::wstring windowText = m_Title + L": " + text;
	SetWindowText(Win32Application::GetHwnd(), windowText.c_str());
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
}

void BaseApplication::EndResize()
{
	if (m_Width != m_PreviousWidth || m_Height != m_PreviousHeight)
	{
		OnResized();
	}
}
