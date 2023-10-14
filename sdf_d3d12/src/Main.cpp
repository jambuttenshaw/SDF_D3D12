#include "pch.h"

#include "Application/D3D12Application.h"
#include "Windows/Win32Application.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	D3D12Application app(1280, 720, L"D3D12 Application");
	return Win32Application::Run(&app, hInstance, nCmdShow);
}
