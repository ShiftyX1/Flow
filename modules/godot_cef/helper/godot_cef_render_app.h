/**************************************************************************/
/*  godot_cef_render_app.h                                                */
/**************************************************************************/

#pragma once

#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"

#include <map>
#include <string>

class GodotCefRenderApp : public CefApp, public CefRenderProcessHandler {
	// All CefRenderProcessHandler callbacks run on the renderer main thread.
	std::map<int, std::string> preload_scripts;

public:
	void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> p_registrar) override;
	CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

	void OnBrowserCreated(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefDictionaryValue> p_extra_info) override;
	void OnBrowserDestroyed(CefRefPtr<CefBrowser> p_browser) override;
	void OnContextCreated(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefRefPtr<CefV8Context> p_context) override;
	void OnFocusedNodeChanged(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefRefPtr<CefDOMNode> p_node) override;
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefProcessId p_source_process, CefRefPtr<CefProcessMessage> p_message) override;

	IMPLEMENT_REFCOUNTING(GodotCefRenderApp);
};
