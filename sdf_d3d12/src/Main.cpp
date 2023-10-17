#include "pch.h"

#include "Application/D3D12Application.h"
#include "Windows/Win32Application.h"

// Different subsystems are used for debug vs release
// This is to enable a console window for logging purposes on debug mode

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif

{
	D3D12Application app(1280, 720, L"D3D12 Application");
	return Win32Application::Run(&app);
}
