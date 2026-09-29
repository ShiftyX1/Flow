/**************************************************************************/
/*  godot_cef_helper_main.cpp                                             */
/**************************************************************************/

// Entry point of the CEF subprocess (renderer, GPU, utility...). Contains no engine code.

#include "godot_cef_render_app.h"

#include "include/cef_app.h"

#if defined(__APPLE__)
#include "include/wrapper/cef_library_loader.h"
#endif

#if defined(_WIN32)
#include <windows.h>

// The engine requests the discrete GPU on hybrid laptops; the helper must pick the same one.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[]) {
#if defined(__APPLE__)
	CefScopedLibraryLoader library_loader;
	if (!library_loader.LoadInHelper()) {
		return 1;
	}
	CefMainArgs main_args(argc, argv);
#elif defined(_WIN32)
	(void)argc;
	(void)argv;
	CefMainArgs main_args(GetModuleHandle(nullptr));
#endif

	CefRefPtr<CefApp> app = new GodotCefRenderApp();
	return CefExecuteProcess(main_args, app, nullptr);
}
