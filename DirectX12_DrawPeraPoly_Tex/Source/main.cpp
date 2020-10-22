#include <iostream>
#include <Windows.h>
#include "Application.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	auto& App = Application::Instance();

	if (!App.Init())
	{
		return 0;
	}

	App.Run();

	App.Terminate();
}