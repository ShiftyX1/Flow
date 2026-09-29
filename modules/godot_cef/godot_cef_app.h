/**************************************************************************/
/*  godot_cef_app.h                                                       */
/**************************************************************************/

#pragma once

#include "godot_cef_include.h"

#include <string>
#include <utility>
#include <vector>

// Snapshot of project settings taken on the main thread before CefInitialize, so CEF
// callbacks never touch engine singletons from other threads.
struct GodotCefAppConfig {
	std::vector<std::pair<std::string, std::string>> switches;
	bool disable_gpu_compositing = false;
};

class GodotCefApp : public CefApp, public CefBrowserProcessHandler {
	GodotCefAppConfig config;

public:
	explicit GodotCefApp(const GodotCefAppConfig &p_config);

	static GodotCefAppConfig make_config_from_settings();

	// CefApp
	void OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line) override;
	void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
	CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

	// CefBrowserProcessHandler
	void OnContextInitialized() override;
	void OnScheduleMessagePumpWork(int64_t delay_ms) override;

	IMPLEMENT_REFCOUNTING(GodotCefApp);
};
