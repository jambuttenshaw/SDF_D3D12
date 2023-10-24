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

#include "imgui.h"
#include "Application/BaseApplication.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


HWND Win32Application::m_hwnd = nullptr;

int Win32Application::Run(BaseApplication* pApp)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Parse the command line parameters
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    pApp->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(pApp->GetWidth()), static_cast<LONG>(pApp->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pApp->GetTitle(),
        WS_OVERLAPPEDWINDOW,
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

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto pApp = reinterpret_cast<BaseApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    // TODO: Win32Application assumes that ImGui is in use, may be a more graceful way of handling this?
    ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);

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
