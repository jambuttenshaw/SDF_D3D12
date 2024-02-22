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

#include "pch.h"
#include "Win32Application.h"

#include "Core.h"
#include "imgui.h"
#include "Application/BaseApplication.h"

HWND Win32Application::m_hwnd = nullptr;
bool Win32Application::m_fullscreenMode = false;
RECT Win32Application::m_windowRect;
using Microsoft::WRL::ComPtr;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


int Win32Application::Run(BaseApplication* pApp)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Parse the command line parameters
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const bool success = pApp->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    if (!success)
    {
        return 0;
    }

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"SDFD3D12";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(pApp->GetWidth()), static_cast<LONG>(pApp->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pApp->GetTitle(),
        m_windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        pApp);

    // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
    pApp->OnInit();

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    pApp->OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}


void Win32Application::MoveCursorToPos(INT x, INT y)
{
    POINT p{ x, y };
    ClientToScreen(m_hwnd, &p);
    SetCursorPos(p.x, p.y);
}


void Win32Application::ToggleFullscreenWindow(IDXGISwapChain* pSwapChain)
{
    if (m_fullscreenMode)
    {
        // Restore the window's attributes and size.
        SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle);

        SetWindowPos(
            m_hwnd,
            HWND_NOTOPMOST,
            m_windowRect.left,
            m_windowRect.top,
            m_windowRect.right - m_windowRect.left,
            m_windowRect.bottom - m_windowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(m_hwnd, SW_NORMAL);
    }
    else
    {
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(m_hwnd, &m_windowRect);

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT fullscreenWindowRect;
        try
        {
            if (pSwapChain)
            {
                // Get the settings of the display on which the app's window is currently displayed
                ComPtr<IDXGIOutput> pOutput;
                THROW_IF_FAIL(pSwapChain->GetContainingOutput(&pOutput));
                DXGI_OUTPUT_DESC Desc;
                THROW_IF_FAIL(pOutput->GetDesc(&Desc));
                fullscreenWindowRect = Desc.DesktopCoordinates;
            }
            else
            {
                // Fallback to EnumDisplaySettings implementation
                throw DXException(S_FALSE);
            }
        }
        catch (DXException& e)
        {
            UNREFERENCED_PARAMETER(e);

            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreenWindowRect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
            };
        }

        SetWindowPos(
            m_hwnd,
            HWND_TOPMOST,
            fullscreenWindowRect.left,
            fullscreenWindowRect.top,
            fullscreenWindowRect.right,
            fullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);


        ShowWindow(m_hwnd, SW_MAXIMIZE);
    }

    m_fullscreenMode = !m_fullscreenMode;
}


// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto pApp = reinterpret_cast<BaseApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    // Check if ImGui wants to handle this event
    if (const LRESULT result = ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return result;
    }

    switch (message)
    {
    case WM_CREATE:
        {
            // Save the BaseApplication* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;
    // Key events
    case WM_SYSKEYDOWN:
        // Handle ALT+ENTER:
        if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
        {
            if (pApp && pApp->GetTearingSupport())
            {
                ToggleFullscreenWindow(pApp->GetSwapChain());
                return 0;
            }
        }
        // Send all other WM_SYSKEYDOWN messages to the default WndProc.
        break;
    case WM_KEYDOWN:
        if (pApp)
        {
            pApp->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pApp)
        {
            pApp->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    // Mouse events
    case WM_MOUSEMOVE:
        if (pApp)
        {
            pApp->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pApp)
        {
            pApp->OnMouseButtonDown(KEY_LBUTTON);
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (pApp)
        {
            pApp->OnMouseButtonDown(KEY_MBUTTON);
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (pApp)
        {
            pApp->OnMouseButtonDown(KEY_RBUTTON);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pApp)
        {
            pApp->OnMouseButtonUp(KEY_LBUTTON);
        }
        return 0;

    case WM_MBUTTONUP:
        if (pApp)
        {
            pApp->OnMouseButtonUp(KEY_MBUTTON);
        }
        return 0;

    case WM_RBUTTONUP:
        if (pApp)
        {
            pApp->OnMouseButtonUp(KEY_RBUTTON);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (pApp)
        {
            pApp->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;

    // Resize events
    case WM_ENTERSIZEMOVE:
        if (pApp)
        {
            pApp->BeginResize();
        }
        return 0;

    case WM_SIZE:
        if (pApp)
        {
            // Since an enter/exit size move isn't sent
            if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
            {
                pApp->BeginResize();
				pApp->Resize(LOWORD(lParam), HIWORD(lParam));
                pApp->EndResize();
            }
            else
				pApp->Resize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_EXITSIZEMOVE:
        if (pApp)
        {
            pApp->EndResize();
        }
        return 0;

    case WM_PAINT:
        if (pApp)
        {
            pApp->OnUpdate();
            pApp->OnRender();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
