/**************************************************************************/
/*  godot_cef_app.cpp                                                     */
/**************************************************************************/

#include "godot_cef_app.h"

#include "common/godot_cef_ipc_contract.h"
#include "godot_cef_osr.h"
#include "godot_cef_runtime.h"
#include "godot_cef_scheme.h"
#include "godot_cef_settings.h"

static std::string _utf8(const String &p_string) {
	const CharString utf8 = p_string.utf8();
	return std::string(utf8.get_data(), utf8.length());
}

GodotCefApp::GodotCefApp(const GodotCefAppConfig &p_config) :
		config(p_config) {}

GodotCefAppConfig GodotCefApp::make_config_from_settings() {
	GodotCefAppConfig result;
	auto add = [&result](const char *p_name, const String &p_value = String()) {
		result.switches.emplace_back(p_name, _utf8(p_value));
	};

	add("autoplay-policy", "no-user-gesture-required");
	add("no-startup-window");
	add("noerrdialogs");
	add("hide-crash-restore-bubble");
	add("enable-logging", "stderr");
	add("transparent-painting-enabled");
	add("enable-zero-copy");
	add("off-screen-rendering-enabled");
	add("use-views");
#ifdef MACOS_ENABLED
	// Keeps Chromium from prompting for keychain access on first launch.
	add("use-mock-keychain");
#endif

	uint32_t vendor_id = 0;
	uint32_t device_id = 0;
	if (GodotCefAcceleratedBackend::get_gpu_ids(vendor_id, device_id) && vendor_id != 0) {
		// Shared textures only work when Chromium renders on the same adapter as Godot.
		add("gpu-vendor-id", itos(vendor_id));
		add("gpu-device-id", itos(device_id));
	}

	if (GodotCefSettings::is_insecure_content_allowed()) {
		add("allow-running-insecure-content");
	}
	if (GodotCefSettings::is_certificate_errors_ignored()) {
		add("ignore-certificate-errors");
		add("ignore-ssl-errors");
	}
	if (GodotCefSettings::is_web_security_disabled()) {
		add("disable-web-security");
	}
	const String proxy_server = GodotCefSettings::get_proxy_server();
	if (!proxy_server.is_empty()) {
		add("proxy-server", proxy_server);
	}
	const String proxy_bypass = GodotCefSettings::get_proxy_bypass_list();
	if (!proxy_bypass.is_empty()) {
		add("proxy-bypass-list", proxy_bypass);
	}
	const int cache_size_mb = GodotCefSettings::get_cache_size_mb();
	if (cache_size_mb > 0) {
		add("disk-cache-size", itos(int64_t(cache_size_mb) * 1024 * 1024));
	}

	for (String custom : GodotCefSettings::get_custom_switches()) {
		custom = custom.strip_edges();
		while (custom.begins_with("-")) {
			custom = custom.substr(1);
		}
		if (custom.is_empty()) {
			continue;
		}
		const int eq = custom.find_char('=');
		if (eq >= 0) {
			result.switches.emplace_back(_utf8(custom.substr(0, eq)), _utf8(custom.substr(eq + 1)));
		} else {
			result.switches.emplace_back(_utf8(custom), std::string());
		}
	}
	return result;
}

void GodotCefApp::OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line) {
	if (!process_type.empty()) {
		return;
	}
	for (const std::pair<std::string, std::string> &sw : config.switches) {
		if (sw.second.empty()) {
			command_line->AppendSwitch(sw.first);
		} else {
			command_line->AppendSwitchWithValue(sw.first, sw.second);
		}
	}
	if (config.disable_gpu_compositing) {
		command_line->AppendSwitch("disable-gpu-compositing");
	}
}

void GodotCefApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
	GodotCefIpc::register_custom_schemes(registrar);
}

void GodotCefApp::OnContextInitialized() {
	godot_cef_register_scheme_handlers();
}

void GodotCefApp::OnScheduleMessagePumpWork(int64_t delay_ms) {
	GodotCefRuntime::schedule_message_pump_work(delay_ms);
}
