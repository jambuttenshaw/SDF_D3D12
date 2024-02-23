#include "pch.h"

#include "Application/D3DApplication.h"
#include "Windows/Win32Application.h"

// Different subsystems are used for debug vs release
// This is to enable a console window for logging purposes on debug mode

#ifndef NDEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
	Log::Init();

	D3DApplication app(1600, 900, L"SDF D3D12");
	return Win32Application::Run(&app);
}
