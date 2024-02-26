//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

class BaseApplication;


class Win32Application
{
public:
    static int Run(BaseApplication* pSample);
    static HWND GetHwnd() { return m_hwnd; }

    static void ShowCursor(bool show) { ::ShowCursor(show); }
    static void MoveCursorToPos(INT x, INT y);

    static void ToggleFullscreenWindow(IDXGISwapChain* swapChain = nullptr);

    static void ForceQuit() { m_ForceQuit = true; }

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static HWND m_hwnd;
    static bool m_fullscreenMode;
    static const UINT m_windowStyle = WS_OVERLAPPEDWINDOW;
    static RECT m_windowRect;

    static bool m_ForceQuit;
};
