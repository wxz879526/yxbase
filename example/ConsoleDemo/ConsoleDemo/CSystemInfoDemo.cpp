#include "pch.h"
#include "CSystemInfoDemo.h"
#include <ShellScalingApi.h>
#include "base/sys_info.h"
#include "ui/display/screen.h"
#include "ui/display/display.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/dpi.h"
#include <iostream>
#include <ShellScalingApi.h>

CSystemInfoDemo::CSystemInfoDemo()
{
}


CSystemInfoDemo::~CSystemInfoDemo()
{
}

bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value)
{
	typedef HRESULT(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
	SetProcessDpiAwarenessPtr set_process_dpi_awareness_func = reinterpret_cast<SetProcessDpiAwarenessPtr>
		(GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessInternal"));

	if (set_process_dpi_awareness_func)
	{
		HRESULT hr = set_process_dpi_awareness_func(value);
		if (SUCCEEDED(hr))
		{
			VLOG(1) << "SetProcessDpiAwarenessWrapper succeeded.";
			return true;
		}
		else if (hr == E_ACCESSDENIED)
		{
			LOG(ERROR) << "Access denied error";
		}
	}

	return false;
}

BOOL SetProcessDPIAwareWrapper()
{
	typedef BOOL(WINAPI* SetProcessDPIAwarePtr)(VOID);
	SetProcessDPIAwarePtr func = reinterpret_cast<SetProcessDPIAwarePtr>(GetProcAddress(
		GetModuleHandleA("user32.dll"), "SetProcessDPIAware"));

	return func && func();
}

void EnableHighDPIAware()
{
	PROCESS_DPI_AWARENESS process_dpi_awareness = PROCESS_PER_MONITOR_DPI_AWARE;
	if (!SetProcessDpiAwarenessWrapper(process_dpi_awareness))
	{
		SetProcessDPIAwareWrapper();
	}
}

void CSystemInfoDemo::DoWork()
{
	EnableHighDPIAware();

	display::win::ScreenWin screen;
	float screen_scale = display::win::GetDPIScale();
	gfx::Size screen_size = screen.GetPrimaryDisplaySize();

	int nScreenWidth, nScreenHeight;
	nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	std::cout << "NumberOfProcessors: " << base::SysInfo::NumberOfProcessors() << std::endl;
}
